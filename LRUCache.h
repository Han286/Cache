#pragma once

#include <cstring>
#include <list>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <functional>
#include "CachePolicy.h"

namespace HanCache {

    template<typename Key, typename Value> class LRUCache;

    template<typename Key, typename Value>
    class LRUNode
    {
    private:
        Key key_;
        Value value_;
        size_t accessCount_;
        std::shared_ptr<LRUNode<Key, Value>> prev_;
        std::shared_ptr<LRUNode<Key, Value>> next_;

    public:
        
        LRUNode(Key key, Value value) 
            : key_(key)
            , value_(value)
            , accessCount_(1)
            , prev_(nullptr)
            , next_(nullptr) 
        {}

        Key getKey() const {return key_;}
        Value getValue() const {return value_;}
        size_t getAccessCount() const {return accessCount_;}
        void setValue(const Value& value) {value_ = value;}
        void incrementAccessCount() {accessCount_++;}

        friend class LRUCache<Key, Value>;
    };
    
  
    template<typename Key, typename Value> 
    class LRUCache : public CachePolicy<Key, Value>
    {
    private:
        using LRUNodeType = LRUNode<Key, Value>;
        using NodePtr = std::shared_ptr<LRUNodeType>;
        using NodeMap = std::unordered_map<Key, NodePtr>;

        size_t capacity_;
        NodePtr dummy_;
        NodeMap nodemap_;
        std::mutex mutex_;
     
    private:
        void removeNode(NodePtr node) {
            node->next_->prev_ = node->prev_;
            node->prev_->next_ = node->next_; 
        }

        void moveToFront(NodePtr node) {
            node->next_ = dummy_->next_;
            node->prev_ = dummy_;
            node->prev_->next_ = node;
            node->next_->prev_ = node;
        }

        NodePtr getNode(const Key& key) {
            auto it = nodemap_.find(key);
            if (it == nodemap_.end())
                return nullptr;

            removeNode(it->second);
            moveToFront(it->second);
            return it->second;
        }
   
    public:

        LRUCache(size_t capacity) {
            capacity_ = capacity;
            dummy_ = std::make_shared<LRUNodeType>(Key(), Value());
            dummy_->next_ = dummy_;
            dummy_->prev_ = dummy_;
        }

        ~LRUCache() override = default;

        void put(Key key, Value value) override {
            if (capacity_ <= 0)
                return;
            
            std::lock_guard<std::mutex> lock(mutex_);
            NodePtr node = getNode(key);
            if (node) {
                node->value_ = value;
                return ;
            }

            node = std::make_shared<LRUNodeType>(key, value);
            moveToFront(node);
            nodemap_[key] = node;

            if (nodemap_.size() > capacity_) {
                node = dummy_->prev_;
                removeNode(node);
                nodemap_.erase(node->key_);
            }
        }

        Value get(Key key) override {
            Value value;

            get(key, value);
            return value;
        }

        bool get(Key key, Value& value) override {

            std::lock_guard<std::mutex> lock(mutex_);
            NodePtr node = getNode(key);
            if (node) {
                value = node->value_;
                return true;
            }
            return false;
        }

        void remove(Key key) {
            NodePtr node = getNode(key);
            if (node) {
                removeNode(node);
                nodemap_.erase(node->key_);
            }
        }
    };

    template<typename Key, typename Value>
    class LRUKCache : public LRUCache<Key, Value> 
    {
    private:
        int k_;
        std::unique_ptr<LRUCache<Key, size_t>> historyList_;
    public:
        LRUKCache(int capacity, int historyCapacity, int k)
            : LRUCache<Key, Value>(capacity)
            , historyList_(std::make_unique<LRUCache<Key, size_t>>(historyCapacity))
            , k_(k)
        {}

        Value get(Key key) {
            int historyCounts = historyList_->get(key);
            historyList_->put(key, ++historyCounts);

            return LRUCache<Key, Value>::get(key);
        }

        bool get(Key key, Value& value) {
            int historyCounts = historyList_->get(key);
            historyList_->put(key, ++historyCounts);

            return LRUCache<Key, Value>::get(key, value);
        }

        void put(Key key, Value value) {
            if (LRUCache<Key, Value>::get(key) != "")
                LRUCache<Key, Value>::put(key, value);
            
            int historyCounts = historyList_->get(key);
            historyList_->put(key, ++historyCounts);

            if (historyCounts >= k_) {
                historyList_->remove(key);
                LRUCache<Key, Value>::put(key, value);
            }
        }
    };

    template<typename Key, typename Value>
    class HashLRUCaches
    {
    private:
        size_t capacity_;
        size_t sliceNum_;
        std::vector<std::unique_ptr<LRUCache<Key, Value>>> lruCaches_;

        size_t hash(Key key) {
            std::hash<Key> hashFunc;
            return hashFunc(key);
        }
    
    public:
        HashLRUCaches (size_t capacity, size_t sliceNum)
            : capacity_(capacity), sliceNum_(sliceNum)
        {
            size_t sliceSize = std::ceil(capacity / static_cast<double>(sliceNum));

            lruCaches_.resize(sliceNum);
            
            for (int i = 0; i < sliceNum; i++) {
                lruCaches_[i] = std::make_unique<LRUCache<Key, Value>>(sliceSize);
            }
        }

        void put(Key key, Value value) {
            size_t sliceIndex = hash(key) % sliceNum_;
            lruCaches_[sliceIndex]->put(key, value);
        }

        bool get(Key key, Value& value) {
            size_t sliceIndex = hash(key) % sliceNum_;
            return lruCaches_[sliceIndex]->get(key, value);
        }

        Value get(Key key) {
            size_t sliceIndex = hash(key) % sliceNum_;
            return lruCaches_[sliceIndex]->get(key);
        }    
    };
} // namespace HanCache