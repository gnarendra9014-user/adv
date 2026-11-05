/*
 * Vending Machine - Interview-style Low-Level Design (LLD)
 *
 * Question:
 * Design a Vending Machine system that can:
 * 1. Hold multiple items arranged by slot (e.g., A1, A2, B1) with prices and stock quantities.
 * 2. Accept money (coins/cash).
 * 3. Handle state transitions cleanly using the State Pattern:
 *    - IdleState: Waiting for money insertion.
 *    - HasMoneyState: Money accepted; user can choose an item or cancel/refund.
 *    - DispenseState: Dispenses item and computes remaining change.
 * 4. Refund money if cancelled or if insufficient money is inserted.
 * 5. Handle edge cases like out-of-stock items, invalid slot codes, and insufficient balance.
 */

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

using namespace std;

// Forward declaration
class VendingMachine;

// -------------------------------------------------------------
// Item & Inventory Models
// -------------------------------------------------------------
struct Item {
    string name;
    double price;

    Item() : name(""), price(0.0) {}
    Item(string name, double price) : name(move(name)), price(price) {}
};

struct ItemSlot {
    Item item;
    int quantity;

    ItemSlot() : quantity(0) {}
    ItemSlot(Item item, int quantity) : item(move(item)), quantity(quantity) {}
};

// -------------------------------------------------------------
// State Interface (State Design Pattern)
// -------------------------------------------------------------
class State {
public:
    virtual ~State() = default;
    virtual void insertMoney(VendingMachine& machine, double amount) = 0;
    virtual void selectItem(VendingMachine& machine, const string& slotCode) = 0;
    virtual void dispense(VendingMachine& machine) = 0;
    virtual void refund(VendingMachine& machine) = 0;
};

// -------------------------------------------------------------
// VendingMachine Context
// -------------------------------------------------------------
class VendingMachine {
public:
    VendingMachine();

    void setState(shared_ptr<State> newState) {
        state_ = move(newState);
    }

    shared_ptr<State> getIdleState() const { return idleState_; }
    shared_ptr<State> getHasMoneyState() const { return hasMoneyState_; }
    shared_ptr<State> getDispenseState() const { return dispenseState_; }

    void addBalance(double amount) {
        balance_ += amount;
    }

    double getBalance() const {
        return balance_;
    }

    void resetBalance() {
        balance_ = 0.0;
    }

    void setSelectedSlot(const string& slot) {
        selectedSlot_ = slot;
    }

    string getSelectedSlot() const {
        return selectedSlot_;
    }

    void addItem(const string& slotCode, const string& name, double price, int quantity) {
        inventory_[slotCode] = ItemSlot(Item(name, price), quantity);
    }

    void restockItem(const string& slotCode, int additionalQuantity) {
        if (additionalQuantity <= 0) {
            cout << "Restock quantity must be positive.\n";
            return;
        }

        auto it = inventory_.find(slotCode);
        if (it == inventory_.end()) {
            cout << "Cannot restock. Slot code " << slotCode << " does not exist.\n";
            return;
        }

        it->second.quantity += additionalQuantity;
        cout << "Restocked slot [" << slotCode << "] (" << it->second.item.name 
             << ") by +" << additionalQuantity 
             << ". New stock: " << it->second.quantity << ".\n";
    }

    bool hasItem(const string& slotCode) const {
        return inventory_.find(slotCode) != inventory_.end();
    }

    ItemSlot& getItemSlot(const string& slotCode) {
        return inventory_[slotCode];
    }

    void displayInventory() const {
        cout << "\n--- Vending Machine Inventory ---\n";
        for (const auto& [code, slot] : inventory_) {
            cout << "[" << code << "] " << slot.item.name
                 << " - $" << slot.item.price
                 << " (" << (slot.quantity > 0 ? to_string(slot.quantity) + " left" : "OUT OF STOCK")
                 << ")\n";
        }
        cout << "--------------------------------\n";
    }

    // Public actions routed to current state
    void insertMoney(double amount) {
        state_->insertMoney(*this, amount);
    }

    void selectItem(const string& slotCode) {
        state_->selectItem(*this, slotCode);
    }

    void dispense() {
        state_->dispense(*this);
    }

    void refund() {
        state_->refund(*this);
    }

private:
    shared_ptr<State> idleState_;
    shared_ptr<State> hasMoneyState_;
    shared_ptr<State> dispenseState_;

    shared_ptr<State> state_;

    unordered_map<string, ItemSlot> inventory_;
    double balance_ = 0.0;
    string selectedSlot_ = "";
};

// -------------------------------------------------------------
// Concrete States
// -------------------------------------------------------------
class IdleState : public State {
public:
    void insertMoney(VendingMachine& machine, double amount) override {
        if (amount <= 0) {
            cout << "Invalid money amount.\n";
            return;
        }
        machine.addBalance(amount);
        cout << "Accepted: $" << amount << ". Total balance: $" << machine.getBalance() << "\n";
        machine.setState(machine.getHasMoneyState());
    }

    void selectItem(VendingMachine& machine, const string& slotCode) override {
        cout << "Please insert money before selecting an item.\n";
    }

    void dispense(VendingMachine& machine) override {
        cout << "No transaction in progress.\n";
    }

    void refund(VendingMachine& machine) override {
        cout << "No balance to refund.\n";
    }
};

class HasMoneyState : public State {
public:
    void insertMoney(VendingMachine& machine, double amount) override {
        if (amount <= 0) {
            cout << "Invalid amount.\n";
            return;
        }
        machine.addBalance(amount);
        cout << "Accepted additional: $" << amount << ". Current balance: $" << machine.getBalance() << "\n";
    }

    void selectItem(VendingMachine& machine, const string& slotCode) override {
        if (!machine.hasItem(slotCode)) {
            cout << "Invalid item code: " << slotCode << ".\n";
            return;
        }

        ItemSlot& slot = machine.getItemSlot(slotCode);
        if (slot.quantity <= 0) {
            cout << "Sorry, item " << slot.item.name << " is out of stock.\n";
            return;
        }

        if (machine.getBalance() < slot.item.price) {
            cout << "Insufficient balance! " << slot.item.name
                 << " costs $" << slot.item.price
                 << ", but balance is only $" << machine.getBalance() << ".\n";
            return;
        }

        machine.setSelectedSlot(slotCode);
        machine.setState(machine.getDispenseState());
        machine.dispense();
    }

    void dispense(VendingMachine& machine) override {
        cout << "Please select an item first.\n";
    }

    void refund(VendingMachine& machine) override {
        cout << "Refunded full amount: $" << machine.getBalance() << "\n";
        machine.resetBalance();
        machine.setState(machine.getIdleState());
    }
};

class DispenseState : public State {
public:
    void insertMoney(VendingMachine& machine, double amount) override {
        cout << "Dispensing in progress. Cannot insert money right now.\n";
    }

    void selectItem(VendingMachine& machine, const string& slotCode) override {
        cout << "Currently dispensing. Please wait.\n";
    }

    void dispense(VendingMachine& machine) override {
        string slotCode = machine.getSelectedSlot();
        ItemSlot& slot = machine.getItemSlot(slotCode);

        // Deduct item and balance
        slot.quantity--;
        double change = machine.getBalance() - slot.item.price;

        cout << "\n>>> Dispensing: " << slot.item.name << " <<<\n";
        if (change > 0.0) {
            cout << "Returning change: $" << change << "\n";
        }

        machine.resetBalance();
        machine.setSelectedSlot("");
        machine.setState(machine.getIdleState());
    }

    void refund(VendingMachine& machine) override {
        cout << "Cannot cancel while dispensing.\n";
    }
};

// VendingMachine constructor implementation
VendingMachine::VendingMachine() {
    idleState_ = make_shared<IdleState>();
    hasMoneyState_ = make_shared<HasMoneyState>();
    dispenseState_ = make_shared<DispenseState>();
    state_ = idleState_;
}

// -------------------------------------------------------------
// Driver Demo
// -------------------------------------------------------------
int main() {
    VendingMachine vm;

    // Setup inventory
    vm.addItem("A1", "Coke", 1.50, 3);
    vm.addItem("A2", "Pepsi", 1.50, 2);
    vm.addItem("B1", "Chips", 1.00, 1);
    vm.addItem("B2", "Chocolate Bar", 2.00, 0); // Out of stock demo

    vm.displayInventory();

    // Scenario 1: Successful purchase with change
    cout << "\n--- Test 1: Successful purchase with change ---\n";
    vm.insertMoney(2.00);
    vm.selectItem("A1");

    // Scenario 2: Insufficient balance then top-up
    cout << "\n--- Test 2: Insufficient balance then top-up ---\n";
    vm.insertMoney(0.50);
    vm.selectItem("B1"); // Chips cost $1.00
    vm.insertMoney(1.00);
    vm.selectItem("B1");

    // Scenario 3: Out of stock & refund
    cout << "\n--- Test 3: Out of stock item and refund ---\n";
    vm.insertMoney(5.00);
    vm.selectItem("B2"); // Chocolate is out of stock
    vm.refund();

    // Scenario 4: Restocking an out-of-stock item and purchasing it
    cout << "\n--- Test 4: Restock item and purchase ---\n";
    vm.restockItem("B2", 5);
    vm.insertMoney(2.00);
    vm.selectItem("B2");

    vm.displayInventory();

    return 0;
}

