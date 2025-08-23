#pragma once

#include <cstdlib>
#include <cstring>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <stdexcept>

// Define PAGE_SIZE if not defined
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif


template <typename T>
class ElementPool
{
public:
  ElementPool()
    : chunkSize_(8192)
  {
    firstChunk_.store(new AtomicPoolChunk(chunkSize_));
  }

  ~ElementPool()
  {
    AtomicPoolChunk *current = firstChunk_.load();
    while (current)
    {
      AtomicPoolChunk *next = current->next;
      delete current;
      current = next;
    }
  }

  // reserve() no-op for compatibility
  void reserve()
  {
    // no-op: atomic pool grows chunks on demand
  }

  inline T *allocate()
  {
    AtomicPoolChunk *chunk = firstChunk_.load();
    while (chunk)
    {
      Slot *head = chunk->freeList.load();
      if (head)
      {
        if (chunk->freeList.compare_exchange_weak(head, head->next))
        {
          return &head->object;
        }
        continue;
      }
      chunk = chunk->next;
    }
    // All chunks full, create a new one
    AtomicPoolChunk *newChunk = new AtomicPoolChunk(chunkSize_);
    newChunk->next = firstChunk_.load();
    while (!firstChunk_.compare_exchange_weak(newChunk->next, newChunk))
      ;
    Slot *slot = newChunk->freeList.load();
    newChunk->freeList.store(slot->next);
    return &slot->object;
  }

  void deallocate(T *ptr)
  {
    AtomicPoolChunk *chunk = firstChunk_.load();
    while (chunk)
    {
      char *start = reinterpret_cast<char *>(chunk->slots);
      char *end = start + chunk->count * sizeof(Slot);
      if (reinterpret_cast<char *>(ptr) >= start && reinterpret_cast<char *>(ptr) < end)
      {
        Slot *slot = reinterpret_cast<Slot *>(ptr);
        Slot *head = chunk->freeList.load();
        do
        {
          slot->next = head;
        } while (!chunk->freeList.compare_exchange_weak(head, slot));
        return;
      }
      chunk = chunk->next;
    }
    // Pointer not from any chunk, ignore or throw
  }

private:
  struct Slot
  {
    T object;
    Slot *next;
  };

  struct AtomicPoolChunk
  {
    size_t count;
    Slot *slots;
    std::atomic<Slot *> freeList;
    AtomicPoolChunk *next;
    AtomicPoolChunk(size_t n)
        : count(n), next(nullptr)
    {
      slots = static_cast<Slot *>(std::malloc(n * sizeof(Slot)));
      if (!slots)
        throw std::bad_alloc();
      freeList.store(nullptr);
      for (size_t i = 0; i < n; ++i)
      {
        slots[i].next = freeList.load();
        freeList.store(&slots[i]);
      }
    }
    ~AtomicPoolChunk()
    {
      std::free(slots);
    }
  };

  std::atomic<AtomicPoolChunk *> firstChunk_;
  size_t chunkSize_;
};

class PagePool
{
public:
    PagePool() = default;
    ~PagePool()
    {
        if (pool_)
        {
            free(pool_);
            capacity_ = 0;
            size_ = 0;
        }
        for (auto p : garbage)
        {
            free(p);
        }
    }

    inline char *allocate()
    {
        // Try atomic free list first using simple stack approach
        uintptr_t head = free_list_head_.load();
        while (head != 0)
        {
            char *page = reinterpret_cast<char *>(head);
            uintptr_t next = *reinterpret_cast<uintptr_t *>(page); // first 8 bytes as next pointer
            if (free_list_head_.compare_exchange_weak(head, next))
            {
                return page;
            }
            // CAS failed, retry with updated head
        }

        // If free list is empty, allocate from pool
        size_t current_size = size_.fetch_add(1);
        if (current_size >= capacity_.load())
        {
            reserve();
            current_size = size_.fetch_add(1);
        }
        return &pool_[current_size * PAGE_SIZE];
    }

    void deallocate(char *page)
    {
        if (page == nullptr)
            return;

        // Add to atomic free list as stack
        uintptr_t page_addr = reinterpret_cast<uintptr_t>(page);
        uintptr_t old_head = free_list_head_.load();
        do
        {
            *reinterpret_cast<uintptr_t *>(page) = old_head; // store next pointer in first 8 bytes
        } while (!free_list_head_.compare_exchange_weak(old_head, page_addr));
    }

    void reserve()
    {
        static std::mutex reserve_mutex;
        std::lock_guard<std::mutex> lock(reserve_mutex);
        if (size_.load() < capacity_.load())
        {
            return;
        }

        if (pool_)
        {
            garbage.push_back(pool_);
        }
        size_t new_capacity = capacity_.load() == 0 ? 131072 : capacity_.load() * 2;
        char *new_pool = (char *)aligned_alloc(4096, new_capacity * PAGE_SIZE);
        if (new_pool == nullptr)
        {
            reserving_.store(false);
            throw std::runtime_error("Failed to allocate memory for PagePool");
        }

        pool_ = new_pool;
        capacity_.store(new_capacity);
        size_.store(0);
    }

private:
    char *pool_ = nullptr;
    std::atomic<size_t> capacity_{0};
    std::atomic<size_t> size_{0};
    std::vector<char *> garbage;

    // Lock-free free list as atomic stack
    std::atomic<uintptr_t> free_list_head_{0}; // 0 means empty
    std::atomic<bool> reserving_{false};
};

// Forward declarations for shared pools
class BasePage;
class DeltaPage;

class SharedMemoryPools
{
public:
    static SharedMemoryPools &getInstance()
    {
        static SharedMemoryPools instance;
        return instance;
    }

    ElementPool<BasePage> &getBasePagePool()
    {
        return base_page_pool_;
    }

    ElementPool<DeltaPage> &getDeltaPagePool()
    {
        return delta_page_pool_;
    }

    PagePool &getPagePool()
    {
        return page_pool_;
    }

private:
    SharedMemoryPools() = default;
    ~SharedMemoryPools() = default;
    SharedMemoryPools(const SharedMemoryPools &) = delete;
    SharedMemoryPools &operator=(const SharedMemoryPools &) = delete;

    ElementPool<BasePage> base_page_pool_;
    ElementPool<DeltaPage> delta_page_pool_;
    PagePool page_pool_;
};
