#pragma once

#include <cstring>
#include <list>
#include <memory>
#include <unordered_map>
#include <mutex>
#include "CachePolicy.h"

namespace HanCache {
    
    template<typename Key, typename Value> class LFUCache;
    template<typename Key, typename Value> class LFUCachePro;

    template<typename Key, typename Value>
    class LFUNode {    
    private:
        Key key;
        Value value;
        size_t accessCount;
        std::shared_ptr<LFUNode<Key, Value>> prev;
        std::shared_ptr<LFUNode<Key, Value>> next;
    
    public:
        LFUNode(Key key, Value value)
            : key(key)
            , value(value)
            , accessCount(1)
            , prev(nullptr)
            , next(nullptr)
        {}

        friend class LFUCache<Key, Value>;
        friend class LFUCachePro<Key, Value>;
    };

    
    
    template<typename Key, typename Value>
    class LFUCache : public CachePolicy<Key, Value> {
    private:
        using LFUNodeType = LFUNode<Key, Value>;
        using NodePtr = std::shared_ptr<LFUNodeType>;
        
        size_t minFreq_;
        size_t capacity_;
        std::mutex mutex_;
        std::unordered_map<Key, NodePtr> keyNodeMap_;
        std::unordered_map<size_t, NodePtr> freqNodeMap_;

    private:

        void removeNode(NodePtr x) {
            x->prev->next = x->next;
            x->next->prev = x->prev;
        }

        void moveToFront(size_t freq, NodePtr x) {
            auto it = freqNodeMap_.find(freq);
            if (it == freqNodeMap_.end()) {
                freqNodeMap_[freq] = addNewNode();
            }
            NodePtr dummy = freqNodeMap_[freq];
            x->next = dummy->next;
            x->prev = dummy;
            x->next->prev = x;
            x->prev->next = x;
        }

        NodePtr addNewNode() {
            NodePtr dummy = std::make_shared<LFUNodeType>(Key(), Value());
            dummy->next = dummy;
            dummy->prev = dummy;
            return dummy;
        }

        NodePtr getNode(Key key) {
            auto it = keyNodeMap_.find(key);
            if (it == keyNodeMap_.end())
                return nullptr;
            
            NodePtr node = it->second;
            if (node->accessCount == minFreq_ && node->prev == node->next)
                minFreq_++;
            removeNode(node);
            moveToFront(++node->accessCount, node);
            return node;
        }
    
    public:
        LFUCache(size_t capacity)
            : capacity_(capacity)
        {}

        ~LFUCache() override = default;
        
        bool get(Key key, Value& value) override {
            if (capacity_ <= 0)
                return false;

            std::lock_guard<std::mutex> lock(mutex_);
            NodePtr node = getNode(key);
            if (node) {
                value = node->value;
                return true;
            }
            return false;
        }

        Value get(Key key) override {
            Value value;
            get(key, value);
            return value;
        }

        void put(Key key, Value value) override {
            
            std::lock_guard<std::mutex> lock(mutex_);
            NodePtr node = getNode(key);
            if (node) {
                node->value = value;
                return;
            }

            if (keyNodeMap_.size() == capacity_) {
                node = freqNodeMap_[minFreq_]->prev;
                removeNode(node);
                keyNodeMap_.erase(node->key);
            }

            node = std::make_shared<LFUNodeType>(key, value);
            moveToFront(1, node);
            keyNodeMap_[key] = node;
            minFreq_ = 1;
        }
    };

    template<typename Key, typename Value>
    class LFUCachePro : CachePolicy<Key, Value> {
    private:
        using LFUNodeType = LFUNode<Key, Value>;
        using NodePtr = std::shared_ptr<LFUNodeType>;
        
        size_t minFreq_;
        size_t capacity_;
        size_t maxAverageNum_;      // new item
        size_t curAverageNum_;      // new item
        size_t curTotalNum_;        // new item
        std::mutex mutex_;
        std::unordered_map<Key, NodePtr> keyNodeMap_;
        std::unordered_map<size_t, NodePtr> freqNodeMap_;

    private:
        void removeNode(NodePtr x) {
            x->prev->next = x->next;
            x->next->prev = x->prev;
        }

        void moveToFront(size_t freq, NodePtr x) {
            auto it = freqNodeMap_.find(freq);
            if (it == freqNodeMap_.end()) {
                freqNodeMap_[freq] = addNewNode();
            }
            NodePtr dummy = freqNodeMap_[freq];
            x->next = dummy->next;
            x->prev = dummy;
            x->next->prev = x;
            x->prev->next = x;
        }

        NodePtr addNewNode() {
            NodePtr dummy = std::make_shared<LFUNodeType>(Key(), Value());
            dummy->next = dummy;
            dummy->prev = dummy;
            return dummy;
        }

        NodePtr getNode(Key key) {
            auto it = keyNodeMap_.find(key);
            if (it == keyNodeMap_.end())
                return nullptr;
            
            NodePtr node = it->second;
            if (node->accessCount == minFreq_ && node->prev == node->next)
                minFreq_++;
            removeNode(node);
            moveToFront(++node->accessCount, node);
            increaseFreqNum();
            return node;
        }
    
        void increaseFreqNum() {
            curTotalNum_++;
            curAverageNum_ = curTotalNum_ / keyNodeMap_.size();
            if (curAverageNum_ > maxAverageNum_) {
                handleFreqOverflow();
            }
        }

        void decreaseFreqNum(size_t num) {
            curTotalNum_ -= num;
            curAverageNum_ = curTotalNum_ / keyNodeMap_.size();
        }

        void handleFreqOverflow() {
            size_t updateMinFreq = SIZE_MAX;
            size_t updateTotalNum = 0;

            for (auto it = keyNodeMap_.begin(); it != keyNodeMap_.end(); it++) {
                NodePtr node = it->second;

                removeNode(node);
                node->accessCount -= maxAverageNum_ / 2;
                if (node->accessCount < 1) node->accessCount = 1;
                updateMinFreq = std::min(updateMinFreq, node->accessCount);
                updateTotalNum += node->accessCount;
                moveToFront(node->accessCount, node);
            }
            minFreq_ = updateMinFreq;
            curTotalNum_ = updateTotalNum;
            curAverageNum_ = curTotalNum_ / keyNodeMap_.size();
        }   

    public:
         LFUCachePro(size_t capacity, size_t maxAverageNum = 20)
            : capacity_(capacity)
            , maxAverageNum_(maxAverageNum)
            , curTotalNum_(0)
            , curAverageNum_(0)
        {}

        ~LFUCachePro() override = default;
        
        bool get(Key key, Value& value) override {
            if (capacity_ <= 0)
                return false;

            std::lock_guard<std::mutex> lock(mutex_);
            NodePtr node = getNode(key);
            if (node) {
                value = node->value;
                return true;
            }
            return false;
        }

        Value get(Key key) override {
            Value value;
            get(key, value);
            return value;
        }

        void put(Key key, Value value) override {
            
            std::lock_guard<std::mutex> lock(mutex_);
            NodePtr node = getNode(key);
            if (node) {
                node->value = value;
                return;
            }

            if (keyNodeMap_.size() == capacity_) {
                node = freqNodeMap_[minFreq_]->prev;
                removeNode(node);
                keyNodeMap_.erase(node->key);
                decreaseFreqNum(node->accessCount);
            }

            node = std::make_shared<LFUNodeType>(key, value);
            moveToFront(1, node);
            keyNodeMap_[key] = node;
            minFreq_ = 1;
            increaseFreqNum();
        }
    
    };
} // namespace end