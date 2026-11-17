/*
 * In-Memory Key-Value Store with Transactions - Interview-Style Low-Level Design (LLD)
 *
 * Question:
 * Design and implement an In-Memory Key-Value database (similar to Redis transactions)
 * that supports:
 *   1. Basic operations:
 *      - SET key value
 *      - GET key
 *      - DELETE key
 *      - COUNT value (number of keys currently set to that value)
 *   2. Nested Transactions (ACID properties for single-node in-memory store):
 *      - BEGIN: Start a new transaction block.
 *      - COMMIT: Commit all open transactions to the active store.
 *      - ROLLBACK: Undo changes made within the current transaction block.
 *
 * Key Design Points:
 *   - Stack of Transaction Frames: Each frame maintains a delta/overlay map of changes
 *     and a reverse-rollback log (or previous states) to support rollback in O(1) or O(K).
 *   - Value Frequency Index: Maintains an inverted index (value -> count) updated in O(1)
 *     for constant-time COUNT queries.
 */

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <stack>

using namespace std;

// -------------------------------------------------------------
// Transaction Frame (delta snapshot for nested transactions)
// -------------------------------------------------------------
struct TransactionFrame {
    // Stores the old state of keys modified during this transaction:
    // nullopt indicates the key did not exist before this transaction.
    unordered_map<string, optional<string>> originalValues;
};

// -------------------------------------------------------------
// In-Memory Key-Value Store
// -------------------------------------------------------------
class KeyValueStore {
public:
    KeyValueStore() = default;

    // Sets key to value. Tracks changes if a transaction is active.
    void set(const string& key, const string& value) {
        auto oldVal = internalGet(key);

        // If inside an active transaction, capture old value once per frame
        if (inTransaction()) {
            auto& frame = transactionStack_.top();
            if (frame.originalValues.find(key) == frame.originalValues.end()) {
                frame.originalValues[key] = oldVal;
            }
        }

        // Update value count index
        if (oldVal.has_value()) {
            decrementValueCount(oldVal.value());
        }
        incrementValueCount(value);

        store_[key] = value;
    }

    // Retrieves value for a key. Returns nullopt if not present.
    optional<string> get(const string& key) const {
        return internalGet(key);
    }

    // Deletes key. Tracks change if a transaction is active.
    bool del(const string& key) {
        auto oldVal = internalGet(key);
        if (!oldVal.has_value()) {
            return false;
        }

        if (inTransaction()) {
            auto& frame = transactionStack_.top();
            if (frame.originalValues.find(key) == frame.originalValues.end()) {
                frame.originalValues[key] = oldVal;
            }
        }

        decrementValueCount(oldVal.value());
        store_.erase(key);
        return true;
    }

    // Returns number of keys that currently hold the given value in O(1).
    int count(const string& value) const {
        auto it = valueCounts_.find(value);
        return (it != valueCounts_.end()) ? it->second : 0;
    }

    // Begins a new transaction block.
    void begin() {
        transactionStack_.push(TransactionFrame{});
        cout << "BEGIN transaction (Depth: " << transactionStack_.size() << ")\n";
    }

    // Rolls back changes in the current transaction block.
    bool rollback() {
        if (!inTransaction()) {
            cout << "ROLLBACK failed: No transaction in progress.\n";
            return false;
        }

        auto frame = move(transactionStack_.top());
        transactionStack_.pop();

        // Restore keys modified in this frame
        for (const auto& [key, oldValOpt] : frame.originalValues) {
            auto currentVal = internalGet(key);
            if (currentVal.has_value()) {
                decrementValueCount(currentVal.value());
            }

            if (oldValOpt.has_value()) {
                store_[key] = oldValOpt.value();
                incrementValueCount(oldValOpt.value());
            } else {
                store_.erase(key);
            }
        }

        cout << "ROLLBACK completed (Remaining Depth: " << transactionStack_.size() << ")\n";
        return true;
    }

    // Commits all open transactions.
    bool commit() {
        if (!inTransaction()) {
            cout << "COMMIT failed: No transaction in progress.\n";
            return false;
        }

        // Changes are already reflected in store_, clear transaction history
        while (!transactionStack_.empty()) {
            transactionStack_.pop();
        }

        cout << "COMMIT completed. All changes finalized.\n";
        return true;
    }

    // Helper: checks if a transaction is currently open
    bool inTransaction() const {
        return !transactionStack_.empty();
    }

    // Helper: prints the current snapshot of store
    void printStore() const {
        cout << "--- Store State ---\n";
        if (store_.empty()) {
            cout << "  (empty)\n";
        } else {
            for (const auto& [k, v] : store_) {
                cout << "  " << k << " = " << v << "\n";
            }
        }
        cout << "-------------------\n";
    }

private:
    optional<string> internalGet(const string& key) const {
        auto it = store_.find(key);
        if (it != store_.end()) {
            return it->second;
        }
        return nullopt;
    }

    void incrementValueCount(const string& value) {
        valueCounts_[value]++;
    }

    void decrementValueCount(const string& value) {
        auto it = valueCounts_.find(value);
        if (it != valueCounts_.end()) {
            it->second--;
            if (it->second <= 0) {
                valueCounts_.erase(it);
            }
        }
    }

    unordered_map<string, string> store_;
    unordered_map<string, int> valueCounts_;
    stack<TransactionFrame> transactionStack_;
};

// -------------------------------------------------------------
// Driver Demo
// -------------------------------------------------------------
int main() {
    KeyValueStore kv;

    cout << "====================================================\n";
    cout << "   Transactional Key-Value Store - System Design    \n";
    cout << "====================================================\n\n";

    // 1. Basic CRUD and COUNT
    cout << "--- Test 1: Basic Operations ---\n";
    kv.set("a", "10");
    kv.set("b", "10");
    kv.set("c", "20");
    kv.printStore();

    cout << "COUNT 10: " << kv.count("10") << " (expected 2)\n";
    cout << "COUNT 20: " << kv.count("20") << " (expected 1)\n";

    kv.del("a");
    cout << "COUNT 10 after deleting 'a': " << kv.count("10") << " (expected 1)\n";

    // 2. Simple Transaction Rollback
    cout << "\n--- Test 2: Simple Transaction Rollback ---\n";
    kv.begin();
    kv.set("b", "99");
    kv.set("d", "100");
    cout << "Inside tx, b = " << kv.get("b").value_or("null") << "\n";
    cout << "COUNT 99: " << kv.count("99") << " (expected 1)\n";

    kv.rollback();
    cout << "After rollback:\n";
    cout << "b = " << kv.get("b").value_or("null") << " (expected 10)\n";
    cout << "d = " << kv.get("d").value_or("null") << " (expected null)\n";
    cout << "COUNT 99: " << kv.count("99") << " (expected 0)\n";

    // 3. Nested Transactions with Commit
    cout << "\n--- Test 3: Nested Transactions ---\n";
    kv.begin(); // Level 1
    kv.set("x", "foo");

    kv.begin(); // Level 2
    kv.set("x", "bar");
    kv.set("y", "baz");
    cout << "Level 2: x = " << kv.get("x").value_or("null") << " (expected bar)\n";

    kv.rollback(); // Undo Level 2
    cout << "After rolling back Level 2:\n";
    cout << "x = " << kv.get("x").value_or("null") << " (expected foo)\n";
    cout << "y = " << kv.get("y").value_or("null") << " (expected null)\n";

    kv.commit(); // Finalize Level 1
    cout << "After commit: x = " << kv.get("x").value_or("null") << "\n";
    kv.printStore();

    return 0;
}
