#pragma once

#include <list>
#include <memory>
#include <cstdio>

namespace xxw {

template <class Key, class Value>
class unordered_map {
private:
    typedef std::pair<Key, Value> DataNode;

    static const int MAX_SIZE = 1e7 + 5;
    static const int MAX_HASH_BASE = 1e5 + 7;
    std::list<DataNode> data_;
    DataNode null_data_;
    std::list<DataNode*> link_table_[MAX_HASH_BASE];

public:
    unordered_map() {}

    ~unordered_map() { clear(); }

    void clear() {
        for(int i = 0; i < MAX_HASH_BASE; ++i) {
            link_table_[i].clear();
        }
        data_.clear();
    }

    inline size_t size() { return data_.size(); }

    bool insert(const Key& key, const Value& value) {
        size_t hash_val = std::hash<Key>()(key) % MAX_HASH_BASE;
        auto& link_node = link_table_[hash_val];
        for (auto& node_ptr : link_node) {
            assert (node_ptr != nullptr);
            if (node_ptr->first == key) {
                node_ptr->second = value;
                return false;
            }
        }
        data_.push_back(DataNode(key, value));
        link_node.push_back(&data_.back());
        return true;
    }

    bool contains(const Key& key) const {
        size_t hash_val = std::hash<Key>()(key) % MAX_HASH_BASE;
        auto& link_node = link_table_[hash_val];
        for (auto& node_ptr : link_node) {
            if (node_ptr->first == key) return true;
        }
        return false;
    }

    Value operator[](const Key& key) const {
        size_t hash_val = std::hash<Key>()(key) % MAX_HASH_BASE;
        auto& link_node = link_table_[hash_val];
        for (auto& node_ptr : link_node) {
            assert (node_ptr != nullptr);
            if (node_ptr->first == key) return node_ptr->second;
        }
        return null_data_.second;
    }

    Value& operator[](const Key& key) {
        size_t hash_val = std::hash<Key>()(key) % MAX_HASH_BASE;
        auto& link_node = link_table_[hash_val];
        for (auto& node_ptr : link_node) {
            assert (node_ptr != nullptr);
            if (node_ptr->first == key) return node_ptr->second;
        }
        data_.push_back(DataNode(key, null_data_.second));
        link_node.push_back(&data_.back());
        return link_node.back()->second;
    }

    bool remove(const Key& key) {
        size_t hash_val = std::hash<Key>()(key) % MAX_HASH_BASE;
        auto& link_node = link_table_[hash_val];
        for (auto& node_ptr : link_node) {
            assert (node_ptr != nullptr);
            if (node_ptr->first == key) {
                link_node.remove(node_ptr);
                data_.remove(*node_ptr);
                return true;
            }
        }

        return false;
    }
};

} // namespace xxw