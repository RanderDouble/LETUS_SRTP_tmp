#include "Worker.hpp"
#include "Master.hpp"
//DeltaPage* Worker::GetDeltaPage(const string& pid) {
//    auto it = active_deltapages_.find(pid);
//    if (it != active_deltapages_.end()) {
//        return it->second;  // return deltapage if it exists
//    }
//    else {
//        DeltaPage* new_page = new DeltaPage(page_pool_.allocate(), pid);
//        new_page->SetLastPageKey(PageKey{ 0, 0, false, pid });
//        active_deltapages_[pid] = new_page;
//        return active_deltapages_[pid];
//    }
//}

pair<uint64_t, uint64_t>  Worker::GetPageVersion(PageKey pagekey) {
    auto it = page_versions_.find(pagekey.pid); //跟据pid寻找version
    if (it != page_versions_.end()) {
        return it->second;
    }
    return { 0, 0 };
}
PageKey Worker::GetLatestBasePageKey(PageKey pagekey) const {
  auto it = page_versions_.find(pagekey.pid);
  if (it != page_versions_.end()) {
    return {it->second.second, pagekey.tid, false, pagekey.pid};
  }
  return PageKey{0, 0, false, pagekey.pid};
}
void  Worker::UpdatePageVersion(PageKey pagekey, uint64_t current_version, uint64_t latest_basepage_version) {
        page_versions_[pagekey.pid] = { current_version, latest_basepage_version };
}
void  Worker::WritePageCache(PageKey pagekey, Page* page) {
    page_cache_[pagekey] = page;
}
void Worker::AddDeltaPageVersion(const string& pid, uint64_t version){
  deltapage_versions_[pid].push_back(version);
}
uint64_t Worker::GetVersionUpperbound(const string &pid, uint64_t version) {
  if (deltapage_versions_.find(pid) == deltapage_versions_.end()) {
    return 0;  // no deltapage of this pid
  }
  vector<uint64_t> &versions = deltapage_versions_[pid];
  auto it = upper_bound(versions.begin(), versions.end(), version);
  if (it == versions.end()) {
    // no deltapage has version larger than requested
    return current_version_;
  }
  return *it;
}
BasePage* Worker::GetPage(
    const PageKey& pagekey) {  // get a page by its pagekey
        auto it = lru_cache_.find(pagekey);
        if (it != lru_cache_.end()) {  // page is in cache
            // move the accessed page to the front
            pagekeys_.splice(pagekeys_.begin(), pagekeys_, it->second);
            it->second = pagekeys_.begin();  // update iterator
            return it->second->second;
        }
        BasePage* page = page_store_->LoadPage(pagekey);
        if(!page) {
          return nullptr;
        }
        PutPage(pagekey, page);
        return page;
        //return nullptr;
}

void Worker::PutPage(const PageKey &pagekey,
                      BasePage *page) {        // add page to cache
  if (lru_cache_.size() >= max_cache_size_) {  // cache is full
    PageKey last_key = pagekeys_.back().first;
    auto last_iter = lru_cache_.find(last_key);
    // 使用池释放机制而不是 delete
    DeletePoolPage(last_iter->second->second);
    // remove the page whose pagekey is at the tail of list
    lru_cache_.erase(last_key);
    pagekeys_.pop_back();
  }
  auto it = lru_cache_.find(pagekey);
  if (it != lru_cache_.end()) {
    // 使用池释放机制而不是 delete
    DeletePoolPage(it->second->second);
    pagekeys_.erase(it->second);
    lru_cache_.erase(it);
  }
  // insert the pair of PageKey and BasePage* to the front
  pagekeys_.push_front(make_pair(pagekey, page));
  lru_cache_[pagekey] = pagekeys_.begin();
}


void  Worker::UpdatePageKey(const PageKey& old_pagekey, const PageKey& new_pagekey) {
    auto it = lru_cache_.find(old_pagekey);
    if (it != lru_cache_.end()) {
        // save the basepage indexed by old pagekey
        BasePage* basepage = it->second->second;
        pagekeys_.erase(it->second);  // delete old pagekey item
        lru_cache_.erase(it);
        pagekeys_.push_front(std::make_pair(new_pagekey, basepage)); //更新lru cache
        lru_cache_[new_pagekey] = pagekeys_.begin();
    }
}

// 统一的页面释放函数
void Worker::DeletePoolPage(Page* page) {
    if (!page)
        return;

    // 先保存 data_ 指针和标志
    char* data = page->GetData();
    bool uses_pool = page->UsesPagePool();

    // 根据页面类型选择对应的池
    if (page->GetPageKey().type) {
        // DeltaPage (type == true)
        DeltaPage* delta_page = dynamic_cast<DeltaPage*>(page);
        if (delta_page) {
            // 调用析构函数（不会释放 PagePool 分配的 data_）
            delta_page->~DeltaPage();
            // 释放对象到池
            pool_delta_.deallocate(delta_page);
        }
    } else {
        // BasePage (type == false)
        BasePage* base_page = dynamic_cast<BasePage*>(page);
        if (base_page) {
            // 调用析构函数（不会释放 PagePool 分配的 data_）
            base_page->~BasePage();
            // 释放对象到池
            pool_.deallocate(base_page);
        }
    }

    // 如果 data_ 来自 PagePool，需要手动释放
    if (uses_pool && data) {
        page_pool_.deallocate(data);
    }
}

void Worker::DeletePoolNode(Node* node) {
    if (!node)
        return;
    if (node->UsesNodePool() == false) {
        delete node;  // 非池分配，直接删除
        return;
    }
    if (node->IsLeaf()) {
        LeafNode* ln = static_cast<LeafNode*>(node);
        ln->~LeafNode();
        pool_leaf_node_.deallocate(ln);
    } else {
        IndexNode* in = static_cast<IndexNode*>(node);
        in->~IndexNode();
        pool_index_node_.deallocate(in);
    }
}