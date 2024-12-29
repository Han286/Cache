#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <iomanip>
#include <random>
#include <algorithm>

#include "LRUCache.h"
#include "LFUCache.h"

using namespace HanCache;

const int OPERATIONS = 100000;
const int HOT_KEYS = 3;
const int COLD_KEYS = 5000;

class Timer {
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}
    
    double elapsed() {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

// 辅助函数：打印结果
void printResults(const std::string& testName, int capacity, int get_operations, int hits) {
    std::cout << "缓存大小: " << capacity << std::endl;
    std::cout << testName << " - 命中率: " << std::fixed << std::setprecision(2) 
              << (100.0 * hits / get_operations) << "%" << std::endl;
}

void testLRUCachePerformance(int capacity) {
    const int CAPACITY = capacity;

    LRUCache<int, std::string> lru(CAPACITY);


    std::random_device rd;
    std::mt19937 gen(rd());
    
    int hits = 0;
    int get_operations = 0;

    // 先进行一系列put操作
    for (int op = 0; op < OPERATIONS; ++op) {
        int key;
        if (op % 100 < 40) {  // 40%热点数据
            key = gen() % HOT_KEYS;
        } else {  // 60%冷数据
            key = HOT_KEYS + (gen() % COLD_KEYS);
        }
        std::string value = "value" + std::to_string(key);
        lru.put(key, value);
    }

    // 然后进行随机get操作
    for (int get_op = 0; get_op < OPERATIONS/2; ++get_op) {
        int key;
        if (get_op % 100 < 40) {  // 40%概率访问热点
            key = gen() % HOT_KEYS;
        } else {  // 60%概率访问冷数据
            key = HOT_KEYS + (gen() % COLD_KEYS);
        }
        
        std::string result;
        get_operations++;
        if (lru.get(key, result)) {
            hits++;
        }
    }

    printResults("LRU Cache 性能测试", CAPACITY, get_operations, hits);
}

void testLFUCachePerformance(int capacity) {
    const int CAPACITY = capacity;

    LFUCache<int, std::string> lfu(CAPACITY);


    std::random_device rd;
    std::mt19937 gen(rd());
    
    int hits = 0;
    int get_operations = 0;

    // 先进行一系列put操作
    for (int op = 0; op < OPERATIONS; ++op) {
        int key;
        if (op % 100 < 40) {  // 40%热点数据
            key = gen() % HOT_KEYS;
        } else {  // 60%冷数据
            key = HOT_KEYS + (gen() % COLD_KEYS);
        }
        std::string value = "value" + std::to_string(key);
        lfu.put(key, value);
    }

    // 然后进行随机get操作
    for (int get_op = 0; get_op < OPERATIONS/2; ++get_op) {
        int key;
        if (get_op % 100 < 40) {  // 40%概率访问热点
            key = gen() % HOT_KEYS;
        } else {  // 60%概率访问冷数据
            key = HOT_KEYS + (gen() % COLD_KEYS);
        }
        
        std::string result;
        get_operations++;
        if (lfu.get(key, result)) {
            hits++;
        }
    }

    printResults("LFU Cache 性能测试", CAPACITY, get_operations, hits);
}

int main(int argc, char* argv[]) {
    testLRUCachePerformance(atoi(argv[1]));
    testLFUCachePerformance(atoi(argv[1]));
    return 0;
}