#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include <unordered_map>
#include <list>
#include <string>
#include <mutex>

class LRUCache {
private:
    size_t capacity_;
    std::list<std::pair<std::string, std::string>> lru_list_;
    std::unordered_map<std::string, decltype(lru_list_.begin())> cache_map_;
    mutable std::mutex mutex_;

public:
    explicit LRUCache(size_t capacity) : capacity_(capacity) {}

    bool Get(const std::string& key, std::string* value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            return false;
        }
        lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
        *value = it->second->second;
        return true;
    }

    void Put(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            it->second->second = value;
            lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
            return;
        }
        if (cache_map_.size() >= capacity_) {
            auto last = lru_list_.end();
            last--;
            cache_map_.erase(last->first);
            lru_list_.pop_back();
        }
        lru_list_.emplace_front(key, value);
        cache_map_[key] = lru_list_.begin();
    }

    size_t Size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_map_.size();
    }
};

#endif // LRU_CACHE_H