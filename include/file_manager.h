#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <string>
#include <fstream>
#include <vector>
#include <map>
#include <algorithm>

// Simple file-based B+ tree implementation for key-value storage
template<typename Key, typename Value, int ORDER = 100>
class BPlusTree {
private:
    struct Node {
        bool is_leaf;
        int key_count;
        Key keys[ORDER];
        union {
            int children[ORDER + 1];  // For internal nodes
            int values[ORDER];         // For leaf nodes (block indices)
            int next_leaf;             // For leaf nodes (next leaf pointer)
        };
        int block_index;

        Node() : is_leaf(true), key_count(0), block_index(-1) {
            for (int i = 0; i <= ORDER; i++) children[i] = -1;
        }
    };

    std::string index_file_;
    std::string data_file_;
    int root_block_;
    int next_block_;
    int next_data_pos_;

    // Simple cache for nodes
    std::map<int, Node> node_cache_;

    Node readNode(int block_index);
    void writeNode(const Node& node);
    int allocateBlock();

    // Find leaf node for key
    int findLeaf(const Key& key);
    int findLeaf(const Key& key, Node& leaf);

    // Insert into leaf
    void insertToLeaf(Node& leaf, const Key& key, int value_pos);
    void insertToInternal(Node& node, int child_index, const Key& key, int right_child);

    // Find value position in data file
    bool findValue(int pos, Value& value);
    int writeValue(const Value& value);

public:
    BPlusTree(const std::string& name);
    ~BPlusTree();

    void initialize();

    // Insert key-value pair
    void insert(const Key& key, const Value& value);

    // Find value by key
    bool find(const Key& key, Value& value);

    // Remove by key
    bool remove(const Key& key);

    // Update value for key
    bool update(const Key& key, const Value& value);

    // Find all values matching a range
    std::vector<Value> rangeFind(const Key& min_key, const Key& max_key);

    // Get all values in order
    std::vector<Value> getAll();

    // Check if tree is empty
    bool empty();

    // Clear all data
    void clear();
};

// Simple file-based storage using linear search (for simplicity and correctness first)
template<typename T>
class SimpleStorage {
private:
    std::string filename_;
    bool initialized_;

public:
    SimpleStorage(const std::string& filename);
    ~SimpleStorage();

    void initialize();

    // Write a record, returns position
    int write(const T& record);

    // Read a record from position
    bool read(int pos, T& record);

    // Update record at position
    bool update(int pos, const T& record);

    // Get all records
    std::vector<T> getAll();

    // Count records
    int count();

    // Clear all records
    void clear();
};

#endif // FILE_MANAGER_H
