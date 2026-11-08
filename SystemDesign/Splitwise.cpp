/*
 * Splitwise (Expense Sharing System) - Low-Level Design (LLD)
 *
 * Question:
 * Design an expense-sharing application like Splitwise where:
 * 1. Users can register with an ID, name, email, and phone number.
 * 2. A user can add an expense paid by them and split among multiple participants.
 * 3. Support multiple split types using the Strategy Pattern:
 *    - EQUAL: Split equally among participants.
 *    - EXACT: Explicit amounts specified per participant (must sum to total).
 *    - PERCENT: Explicit percentage per participant (must sum to 100%).
 * 4. Maintain a balance sheet tracking who owes whom and how much.
 * 5. Display individual user balances as well as all system balances.
 * 6. Simplify / settle debts between users.
 */

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <iomanip>
#include <cmath>
#include <stdexcept>

using namespace std;

// -------------------------------------------------------------
// Models: User & Split
// -------------------------------------------------------------
class User {
public:
    User(string id, string name, string email, string phone)
        : id_(move(id)), name_(move(name)), email_(move(email)), phone_(move(phone)) {}

    const string& getId() const { return id_; }
    const string& getName() const { return name_; }

private:
    string id_;
    string name_;
    string email_;
    string phone_;
};

enum class SplitType {
    EQUAL,
    EXACT,
    PERCENT
};

struct Split {
    shared_ptr<User> user;
    double amount = 0.0;
    double percent = 0.0;

    Split(shared_ptr<User> u) : user(move(u)) {}
    Split(shared_ptr<User> u, double amt) : user(move(u)), amount(amt) {}
    Split(shared_ptr<User> u, double amt, double pct) : user(move(u)), amount(amt), percent(pct) {}
};

// -------------------------------------------------------------
// Strategy Pattern: Expense Validation & Calculation
// -------------------------------------------------------------
class ExpenseValidator {
public:
    virtual ~ExpenseValidator() = default;
    virtual bool validateAndCompute(double totalAmount, vector<Split>& splits) = 0;
};

class EqualExpenseValidator : public ExpenseValidator {
public:
    bool validateAndCompute(double totalAmount, vector<Split>& splits) override {
        if (splits.empty()) return false;

        double splitAmount = std::round((totalAmount / splits.size()) * 100.0) / 100.0;
        double firstShare = totalAmount - (splitAmount * (splits.size() - 1));

        for (size_t i = 0; i < splits.size(); ++i) {
            splits[i].amount = (i == 0) ? firstShare : splitAmount;
        }
        return true;
    }
};

class ExactExpenseValidator : public ExpenseValidator {
public:
    bool validateAndCompute(double totalAmount, vector<Split>& splits) override {
        double sum = 0.0;
        for (const auto& split : splits) {
            sum += split.amount;
        }
        return std::abs(sum - totalAmount) < 0.01;
    }
};

class PercentExpenseValidator : public ExpenseValidator {
public:
    bool validateAndCompute(double totalAmount, vector<Split>& splits) override {
        double totalPercent = 0.0;
        for (const auto& split : splits) {
            totalPercent += split.percent;
        }
        if (std::abs(totalPercent - 100.0) > 0.01) {
            return false;
        }

        for (auto& split : splits) {
            split.amount = std::round((totalAmount * split.percent / 100.0) * 100.0) / 100.0;
        }
        return true;
    }
};

// -------------------------------------------------------------
// Expense Model
// -------------------------------------------------------------
class Expense {
public:
    Expense(string id, string description, double totalAmount,
            shared_ptr<User> paidBy, SplitType splitType, vector<Split> splits)
        : id_(move(id)), description_(move(description)), totalAmount_(totalAmount),
          paidBy_(move(paidBy)), splitType_(splitType), splits_(move(splits)) {}

    const string& getId() const { return id_; }
    const string& getDescription() const { return description_; }
    double getTotalAmount() const { return totalAmount_; }
    shared_ptr<User> getPaidBy() const { return paidBy_; }
    const vector<Split>& getSplits() const { return splits_; }

private:
    string id_;
    string description_;
    double totalAmount_;
    shared_ptr<User> paidBy_;
    SplitType splitType_;
    vector<Split> splits_;
};

// -------------------------------------------------------------
// Splitwise Service Manager
// -------------------------------------------------------------
class SplitwiseService {
public:
    SplitwiseService() {
        validators_[SplitType::EQUAL] = make_unique<EqualExpenseValidator>();
        validators_[SplitType::EXACT] = make_unique<ExactExpenseValidator>();
        validators_[SplitType::PERCENT] = make_unique<PercentExpenseValidator>();
    }

    void registerUser(const string& id, const string& name, const string& email, const string& phone) {
        if (users_.count(id)) {
            cout << "User with ID " << id << " already exists!\n";
            return;
        }
        users_[id] = make_shared<User>(id, name, email, phone);
        cout << "Registered: " << name << " (ID: " << id << ")\n";
    }

    shared_ptr<User> getUser(const string& userId) const {
        auto it = users_.find(userId);
        return (it != users_.end()) ? it->second : nullptr;
    }

    void addExpense(const string& expenseId, const string& description, double amount,
                    const string& paidById, SplitType splitType, vector<Split> splits) {
        auto payer = getUser(paidById);
        if (!payer) {
            cout << "Payer ID " << paidById << " not found!\n";
            return;
        }

        auto it = validators_.find(splitType);
        if (it == validators_.end() || !it->second->validateAndCompute(amount, splits)) {
            cout << "Invalid split details for expense: " << description << ". Expense rejected.\n";
            return;
        }

        expenses_.push_back(make_shared<Expense>(expenseId, description, amount, payer, splitType, splits));

        // Update balance sheet
        for (const auto& split : splits) {
            const string& borrowerId = split.user->getId();
            if (borrowerId == paidById) continue;

            // paidById is owed money by borrowerId
            balanceSheet_[paidById][borrowerId] += split.amount;
            balanceSheet_[borrowerId][paidById] -= split.amount;
        }

        cout << "Expense '" << description << "' of $" << fixed << setprecision(2)
             << amount << " added successfully.\n";
    }

    void showBalances() const {
        cout << "\n================= ALL BALANCES =================\n";
        bool isEmpty = true;
        for (const auto& [user1, balances] : balanceSheet_) {
            for (const auto& [user2, amount] : balances) {
                if (amount > 0.009) {
                    isEmpty = false;
                    cout << users_.at(user2)->getName() << " owes "
                         << users_.at(user1)->getName() << ": $"
                         << fixed << setprecision(2) << amount << "\n";
                }
            }
        }
        if (isEmpty) {
            cout << "No balances pending! All settled up.\n";
        }
        cout << "================================================\n";
    }

    void showBalanceForUser(const string& userId) const {
        auto user = getUser(userId);
        if (!user) {
            cout << "User " << userId << " not found!\n";
            return;
        }

        cout << "\n--- Balances for " << user->getName() << " ---\n";
        bool hasBalance = false;
        auto it = balanceSheet_.find(userId);
        if (it != balanceSheet_.end()) {
            for (const auto& [otherId, amount] : it->second) {
                if (amount > 0.009) {
                    hasBalance = true;
                    cout << users_.at(otherId)->getName() << " owes you: $"
                         << fixed << setprecision(2) << amount << "\n";
                } else if (amount < -0.009) {
                    hasBalance = true;
                    cout << "You owe " << users_.at(otherId)->getName() << ": $"
                         << fixed << setprecision(2) << std::abs(amount) << "\n";
                }
            }
        }
        if (!hasBalance) {
            cout << "No balances for " << user->getName() << ".\n";
        }
        cout << "------------------------------------------\n";
    }

    void settlePayment(const string& fromUserId, const string& toUserId, double amount) {
        auto fromUser = getUser(fromUserId);
        auto toUser = getUser(toUserId);

        if (!fromUser || !toUser) {
            cout << "Invalid users for settlement.\n";
            return;
        }

        balanceSheet_[fromUserId][toUserId] += amount;
        balanceSheet_[toUserId][fromUserId] -= amount;

        cout << fromUser->getName() << " paid $" << fixed << setprecision(2)
             << amount << " to " << toUser->getName() << ". Balances updated!\n";
    }

    void showExpenseHistory() const {
        cout << "\n================= EXPENSE HISTORY =================\n";
        if (expenses_.empty()) {
            cout << "No expenses recorded yet.\n";
            cout << "===================================================\n";
            return;
        }

        for (const auto& exp : expenses_) {
            cout << "[" << exp->getId() << "] " << exp->getDescription()
                 << " | Total: $" << fixed << setprecision(2) << exp->getTotalAmount()
                 << " | Paid by: " << exp->getPaidBy()->getName() << "\n";
            cout << "  Participants:\n";
            for (const auto& split : exp->getSplits()) {
                cout << "    - " << split.user->getName() << ": $"
                     << fixed << setprecision(2) << split.amount;
                if (split.percent > 0.0) {
                    cout << " (" << setprecision(1) << split.percent << "%)";
                }
                cout << "\n";
            }
        }
        cout << "===================================================\n";
    }

    double getTotalNetBalance(const string& userId) const {
        auto user = getUser(userId);
        if (!user) {
            cout << "User " << userId << " not found!\n";
            return 0.0;
        }

        double netBalance = 0.0;
        auto it = balanceSheet_.find(userId);
        if (it != balanceSheet_.end()) {
            for (const auto& [_, amount] : it->second) {
                netBalance += amount;
            }
        }
        return netBalance;
    }

private:
    unordered_map<string, shared_ptr<User>> users_;
    vector<shared_ptr<Expense>> expenses_;
    unordered_map<SplitType, unique_ptr<ExpenseValidator>> validators_;
    // balanceSheet_[userA][userB] > 0 means userB owes userA that amount
    unordered_map<string, unordered_map<string, double>> balanceSheet_;
};

// -------------------------------------------------------------
// Driver Code
// -------------------------------------------------------------
int main() {
    SplitwiseService service;

    cout << "=== Setting up users ===\n";
    service.registerUser("u1", "Alice", "alice@example.com", "1111111111");
    service.registerUser("u2", "Bob", "bob@example.com", "2222222222");
    service.registerUser("u3", "Charlie", "charlie@example.com", "3333333333");
    service.registerUser("u4", "Diana", "diana@example.com", "4444444444");

    auto u1 = service.getUser("u1");
    auto u2 = service.getUser("u2");
    auto u3 = service.getUser("u3");
    auto u4 = service.getUser("u4");

    // 1. Equal split among 4 users for $1000 dinner paid by Alice
    cout << "\n--- Expense 1: Equal Split ($1000) ---\n";
    vector<Split> equalSplits = { Split(u1), Split(u2), Split(u3), Split(u4) };
    service.addExpense("e1", "Team Dinner", 1000.0, "u1", SplitType::EQUAL, equalSplits);
    service.showBalances();

    // 2. Exact split for $500 shopping paid by Bob
    cout << "\n--- Expense 2: Exact Split ($500) ---\n";
    vector<Split> exactSplits = { Split(u2, 150.0), Split(u3, 350.0) };
    service.addExpense("e2", "Grocery Shopping", 500.0, "u2", SplitType::EXACT, exactSplits);
    service.showBalances();

    // 3. Percent split for $200 cab ride paid by Charlie (Alice 40%, Bob 20%, Charlie 40%)
    cout << "\n--- Expense 3: Percent Split ($200) ---\n";
    vector<Split> percentSplits = { Split(u1, 0, 40.0), Split(u2, 0, 20.0), Split(u3, 0, 40.0) };
    service.addExpense("e3", "Cab Ride", 200.0, "u3", SplitType::PERCENT, percentSplits);
    service.showBalances();

    // Specific user balance
    service.showBalanceForUser("u1");

    // 4. Settle payment
    cout << "\n--- Settlement: Bob pays Alice $250 ---\n";
    service.settlePayment("u2", "u1", 250.0);
    service.showBalanceForUser("u2");
    service.showBalances();

    // 5. Expense history
    service.showExpenseHistory();

    // 6. Total net balance check
    cout << "\n--- Net Balances Summary ---\n";
    for (const string& uid : {"u1", "u2", "u3", "u4"}) {
        auto u = service.getUser(uid);
        double net = service.getTotalNetBalance(uid);
        cout << u->getName() << "'s Net Balance: "
             << (net >= 0 ? "+$" : "-$") << fixed << setprecision(2) << std::abs(net)
             << (net > 0.009 ? " (to receive)" : (net < -0.009 ? " (to pay)" : " (settled)"))
             << "\n";
    }

    return 0;
}

