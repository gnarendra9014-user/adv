/*
 * LRU Cache - Interview-style Low-Level Design (LLD)
 *
 * Question:
 * Design and implement a Least Recently Used (LRU) Cache with:
 *   - A fixed maximum capacity.
 *   - O(1) time complexity for both `get` and `put` operations.
 *   - When the cache is full and a new key is inserted, the least
 *     recently used entry is evicted.
 *
 * Key design points:
 *   - A doubly linked list maintains the access order (most recent at
 *     front, least recent at back).
 *   - An unordered_map provides O(1) key lookups pointing directly
 *     into the doubly linked list.
 *   - On every `get` or `put` that hits an existing key, the node is
 *     moved to the front (most recently used).
 *   - On `put` when the cache is full, the back node (LRU) is evicted.
 *   - Extended with `remove`, `clear`, `displayCache`, and `getSize`
 *     for completeness.
 */

#include <iostream>
#include <list>
#include <string>
#include <unordered_map>

using namespace std;

// -------------------------------------------------------------
// LRUCache  –  Doubly Linked List + Hash Map
// -------------------------------------------------------------
class LRUCache {
public:
    explicit LRUCache(int capacity) : capacity_(capacity) {
        if (capacity <= 0) {
            throw invalid_argument("Cache capacity must be positive.");
        }
        cout << "LRU Cache created with capacity: " << capacity_ << "\n";
    }

    // Returns the value for the given key, or -1 if not found.
    // Marks the accessed key as most recently used.
    int get(int key) {
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            cout << "GET key=" << key << " → MISS\n";
            return -1;
        }

        // Move accessed node to front (most recently used)
        accessOrder_.splice(accessOrder_.begin(), accessOrder_, it->second);

        cout << "GET key=" << key << " → HIT, value=" << it->second->second << "\n";
        return it->second->second;
    }

    // Inserts or updates the key-value pair.
    // If the cache is at capacity, evicts the least recently used entry.
    void put(int key, int value) {
        auto it = cache_.find(key);

        if (it != cache_.end()) {
            // Key exists: update value and move to front
            it->second->second = value;
            accessOrder_.splice(accessOrder_.begin(), accessOrder_, it->second);
            cout << "PUT key=" << key << ", value=" << value << " → UPDATED\n";
            return;
        }

        // Evict LRU entry if cache is full
        if (static_cast<int>(cache_.size()) >= capacity_) {
            auto& lruEntry = accessOrder_.back();
            cout << "  EVICT key=" << lruEntry.first
                 << " (LRU, value=" << lruEntry.second << ")\n";
            cache_.erase(lruEntry.first);
            accessOrder_.pop_back();
        }

        // Insert new entry at the front
        accessOrder_.emplace_front(key, value);
        cache_[key] = accessOrder_.begin();
        cout << "PUT key=" << key << ", value=" << value << " → INSERTED\n";
    }

    // Removes a specific key from the cache.
    bool remove(int key) {
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            cout << "REMOVE key=" << key << " → NOT FOUND\n";
            return false;
        }

        accessOrder_.erase(it->second);
        cache_.erase(it);
        cout << "REMOVE key=" << key << " → REMOVED\n";
        return true;
    }

    // Returns the current number of entries in the cache.
    int getSize() const {
        return static_cast<int>(cache_.size());
    }

    // Returns true if the cache contains the given key.
    bool contains(int key) const {
        return cache_.find(key) != cache_.end();
    }

    // Clears all entries from the cache.
    void clear() {
        cache_.clear();
        accessOrder_.clear();
        cout << "CLEAR → Cache emptied.\n";
    }

    // Displays the cache contents in order from most to least recently used.
    void displayCache() const {
        cout << "\n--- Cache State (MRU → LRU) ---\n";
        if (accessOrder_.empty()) {
            cout << "  [empty]\n";
        } else {
            int rank = 1;
            for (const auto& [key, value] : accessOrder_) {
                cout << "  " << rank++ << ". key=" << key << ", value=" << value << "\n";
            }
        }
        cout << "  Size: " << cache_.size() << " / " << capacity_ << "\n";
        cout << "-------------------------------\n";
    }

private:
    int capacity_;

    // Doubly linked list: front = most recently used, back = least recently used
    list<pair<int, int>> accessOrder_;

    // Hash map: key → iterator into the doubly linked list
    unordered_map<int, list<pair<int, int>>::iterator> cache_;
};

// -------------------------------------------------------------
// Driver Demo
// -------------------------------------------------------------
int main() {
    try {
        LRUCache cache(3);

        // Scenario 1: Basic put and get
        cout << "\n=== Test 1: Basic Put & Get ===\n";
        cache.put(1, 100);
        cache.put(2, 200);
        cache.put(3, 300);
        cache.displayCache();

        cache.get(2);  // Access key 2 → moves to front
        cache.displayCache();

        // Scenario 2: Eviction on capacity overflow
        cout << "\n=== Test 2: Eviction ===\n";
        cache.put(4, 400);  // Should evict key 1 (LRU)
        cache.displayCache();

        cache.get(1);  // Cache miss since key 1 was evicted

        // Scenario 3: Update existing key
        cout << "\n=== Test 3: Update Existing Key ===\n";
        cache.put(3, 999);  // Update key 3, moves to front
        cache.displayCache();

        // Scenario 4: Remove a specific key
        cout << "\n=== Test 4: Remove Key ===\n";
        cache.remove(2);
        cache.displayCache();

        cache.remove(99);  // Key not in cache

        // Scenario 5: Contains check
        cout << "\n=== Test 5: Contains Check ===\n";
        cout << "Contains key 3? " << (cache.contains(3) ? "Yes" : "No") << "\n";
        cout << "Contains key 1? " << (cache.contains(1) ? "Yes" : "No") << "\n";

        // Scenario 6: Clear cache
        cout << "\n=== Test 6: Clear Cache ===\n";
        cache.clear();
        cache.displayCache();

    } catch (const exception& error) {
        cerr << "Error: " << error.what() << "\n";
        return 1;
    }

    return 0;
}

