/*
 * Factory Pattern - interview-style example
 *
 * Question:
 * Design a vehicle creator that returns the correct vehicle object based on
 * the requested type. The caller should not need to know which concrete class
 * is instantiated.
 *
 * The factory centralizes object-creation logic and keeps main() dependent on
 * the Vehicle interface instead of concrete vehicle classes.
 */

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

using namespace std;

class Vehicle {
public:
    virtual ~Vehicle() = default;
    virtual void start() const = 0;
};

class Car final : public Vehicle {
public:
    void start() const override {
        cout << "Car engine started.\n";
    }
};

class Bike final : public Vehicle {
public:
    void start() const override {
        cout << "Bike engine started.\n";
    }
};

class Truck final : public Vehicle {
public:
    void start() const override {
        cout << "Truck engine started.\n";
    }
};

class VehicleFactory {
public:
    static unique_ptr<Vehicle> createVehicle(const string& vehicleType) {
        if (vehicleType == "car") {
            return make_unique<Car>();
        }
        if (vehicleType == "bike") {
            return make_unique<Bike>();
        }
        if (vehicleType == "truck") {
            return make_unique<Truck>();
        }

        throw invalid_argument("Unsupported vehicle type: " + vehicleType);
    }

    static unique_ptr<Vehicle> createDefaultVehicle() {
        return createVehicle("car");
    }
};

int main() {
    const string vehicleType = "bike";

    try {
        const unique_ptr<Vehicle> vehicle =
            VehicleFactory::createVehicle(vehicleType);
        vehicle->start();
    } catch (const invalid_argument& error) {
        cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
