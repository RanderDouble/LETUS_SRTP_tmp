#include "common.hpp"
#include "ElementPool.hpp"
#include <iostream>
#include <cassert>
#include <vector>

struct TestStruct {
    int x;
    double y;
};

void test_ElementPool() {
    ElementPool<TestStruct> pool;
    const int N = 10000;
    std::vector<TestStruct*> ptrs;
    // 批量分配
    for (int i = 0; i < N; ++i) {
        TestStruct* p = pool.allocate();
        assert(p != nullptr);
        p->x = i;
        p->y = i * 1.0;
        ptrs.push_back(p);
    }
    // 检查数据
    for (int i = 0; i < N; ++i) {
        assert(ptrs[i]->x == i);
        assert(ptrs[i]->y == i * 1.0);
    }
    // 批量释放
    for (int i = 0; i < N; ++i) {
        pool.deallocate(ptrs[i]);
    }
    // 再次分配，检查复用
    std::vector<TestStruct*> ptrs2;
    for (int i = 0; i < N; ++i) {
        TestStruct* p = pool.allocate();
        assert(p != nullptr);
        ptrs2.push_back(p);
    }
    std::cout << "ElementPool batch test passed!" << std::endl;
}

void test_PagePool() {
    PagePool pool;
    const int N = 10000;
    std::vector<char*> pages;
    // 批量分配
    for (int i = 0; i < N; ++i) {
        char* p = pool.allocate();
        assert(p != nullptr);
        pages.push_back(p);
    }
    // 批量释放
    for (int i = 0; i < N; ++i) {
        pool.deallocate(pages[i]);
    }
    // 再次分配，检查复用
    std::vector<char*> pages2;
    for (int i = 0; i < N; ++i) {
        char* p = pool.allocate();
        assert(p != nullptr);
        pages2.push_back(p);
    }
    std::cout << "PagePool batch test passed!" << std::endl;
}

int main() {
    test_ElementPool();
    test_PagePool();
    return 0;
} 