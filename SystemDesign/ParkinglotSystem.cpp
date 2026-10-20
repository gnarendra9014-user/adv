#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace std;

class Vehicle {
public:
    string vehicleNumber;
    string vehicleType;
    time_t entryTime;

    Vehicle(const string& number, const string& type)
        : vehicleNumber(number), vehicleType(type), entryTime(time(nullptr)) {}
};

class ParkingLot {
private:
    vector<Vehicle> vehicles;
    int capacity;

    static string normalizeVehicleType(string type) {
        transform(type.begin(), type.end(), type.begin(),
                  [](unsigned char character) {
                      return static_cast<char>(tolower(character));
                  });
        return type;
    }

public:
    explicit ParkingLot(int size) : capacity(size) {}

    void parkVehicle() {
        if (vehicles.size() >= static_cast<size_t>(capacity)) {
            cout << "Parking lot is full!\n";
            return;
        }

        string number;
        cout << "Enter vehicle number: ";
        cin >> number;

        for (const Vehicle& vehicle : vehicles) {
            if (vehicle.vehicleNumber == number) {
                cout << "Vehicle is already parked!\n";
                return;
            }
        }

        string type;
        cout << "Enter vehicle type (car/bike): ";
        cin >> type;
        type = normalizeVehicleType(type);

        if (type != "car" && type != "bike") {
            cout << "Invalid vehicle type. Please enter car or bike.\n";
            return;
        }

        vehicles.emplace_back(number, type);
        cout << "Vehicle parked successfully!\n";
    }

    void removeVehicle() {
        string number;
        cout << "Enter vehicle number to remove: ";
        cin >> number;

        for (auto iterator = vehicles.begin(); iterator != vehicles.end(); ++iterator) {
            if (iterator->vehicleNumber == number) {
                const time_t exitTime = time(nullptr);
                const double hours =
                    difftime(exitTime, iterator->entryTime) / 3600.0;
                const int billedHours = max(1, static_cast<int>(ceil(hours)));
                const int rate = iterator->vehicleType == "bike" ? 20 : 50;
                const int fee = billedHours * rate;

                cout << "Vehicle removed successfully!\n";
                cout << "Parking duration: " << hours << " hours\n";
                cout << "Parking fee: Rs. " << fee << "\n";

                vehicles.erase(iterator);
                return;
            }
        }

        cout << "Vehicle not found!\n";
    }

    void displayVehicles() const {
        if (vehicles.empty()) {
            cout << "No vehicles parked.\n";
            return;
        }

        cout << "\n----- Parked Vehicles -----\n";
        for (const Vehicle& vehicle : vehicles) {
            cout << "Vehicle number: " << vehicle.vehicleNumber << "\n"
                 << "Vehicle type: " << vehicle.vehicleType << "\n"
                 << "Entry time: " << ctime(&vehicle.entryTime);
            cout << "---------------------------\n";
        }
        cout << "Occupied spaces: " << vehicles.size() << "/" << capacity << "\n";
    }

    void searchVehicle() const {
        string number;
        cout << "Enter vehicle number to search: ";
        cin >> number;

        for (const Vehicle& vehicle : vehicles) {
            if (vehicle.vehicleNumber == number) {
                cout << "\nVehicle found!\n"
                     << "Vehicle number: " << vehicle.vehicleNumber << "\n"
                     << "Vehicle type: " << vehicle.vehicleType << "\n"
                     << "Entry time: " << ctime(&vehicle.entryTime);
                return;
            }
        }

        cout << "Vehicle not found in the parking lot.\n";
    }
};

int main() {
    int capacity;
    cout << "Enter parking lot capacity: ";

    while (!(cin >> capacity) || capacity <= 0) {
        cout << "Capacity must be a positive number. Try again: ";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    ParkingLot parkingLot(capacity);
    int choice;

    do {
        cout << "\n===== PARKING LOT SYSTEM =====\n"
             << "1. Park vehicle\n"
             << "2. Remove vehicle\n"
             << "3. Display parked vehicles\n"
             << "4. Search vehicle\n"
             << "0. Exit\n"
             << "Enter your choice: ";

        if (!(cin >> choice)) {
            cout << "Invalid choice. Try again.\n";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            continue;
        }

        switch (choice) {
        case 1:
            parkingLot.parkVehicle();
            break;
        case 2:
            parkingLot.removeVehicle();
            break;
        case 3:
            parkingLot.displayVehicles();
            break;
        case 4:
            parkingLot.searchVehicle();
            break;
        case 0:
            cout << "Exiting system...\n";
            break;
        default:
            cout << "Invalid choice. Try again.\n";
        }
    } while (choice != 0);

    return 0;
}
