/*
 * Ride-Sharing Service (Uber / Lyft) - Interview-Style Low-Level Design (LLD)
 *
 * Question:
 * Design a simplified ride-hailing / ride-sharing system (like Uber or Lyft) that can:
 *   1. Register Riders and Drivers with geographical locations (coordinates).
 *   2. Support Driver Availability: Online / Offline status and active ride status.
 *   3. Matchmaking / Driver Assignment Strategy:
 *      - Nearest Available Driver Strategy (Euclidean distance).
 *   4. Pricing Strategy:
 *      - Standard Pricing (base fare + per-km rate).
 *      - Surge Pricing (multiplier based on demand / peak hours).
 *   5. Ride Lifecycle Management via State Pattern / Finite State Machine:
 *      - REQUESTED -> ACCEPTED -> IN_PROGRESS -> COMPLETED (or CANCELLED).
 *   6. Rating and feedback for rides.
 */

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cmath>
#include <iomanip>
#include <limits>
#include <algorithm>

using namespace std;

// -------------------------------------------------------------
// Location & Core Models
// -------------------------------------------------------------
struct Location {
    double latitude;
    double longitude;

    Location() : latitude(0.0), longitude(0.0) {}
    Location(double lat, double lon) : latitude(lat), longitude(lon) {}

    double distanceTo(const Location& other) const {
        double dLat = latitude - other.latitude;
        double dLon = longitude - other.longitude;
        return sqrt(dLat * dLat + dLon * dLon);
    }
};

enum class RideStatus {
    REQUESTED,
    ACCEPTED,
    IN_PROGRESS,
    COMPLETED,
    CANCELLED
};

class Rider {
public:
    Rider(string id, string name, string phone)
        : id_(move(id)), name_(move(name)), phone_(move(phone)) {}

    const string& getId() const { return id_; }
    const string& getName() const { return name_; }

private:
    string id_;
    string name_;
    string phone_;
};

class Driver {
public:
    Driver(string id, string name, Location location)
        : id_(move(id)), name_(move(name)), location_(location), isAvailable_(true), ratingSum_(0.0), ratingCount_(0) {}

    const string& getId() const { return id_; }
    const string& getName() const { return name_; }
    Location getLocation() const { return location_; }
    void setLocation(Location loc) { location_ = loc; }

    bool isAvailable() const { return isAvailable_; }
    void setAvailable(bool available) { isAvailable_ = available; }

    void addRating(int rating) {
        ratingSum_ += rating;
        ratingCount_++;
    }

    double getAverageRating() const {
        if (ratingCount_ == 0) return 5.0;
        return ratingSum_ / ratingCount_;
    }

private:
    string id_;
    string name_;
    Location location_;
    bool isAvailable_;
    double ratingSum_;
    int ratingCount_;
};

// -------------------------------------------------------------
// Strategy Pattern: Pricing Strategy
// -------------------------------------------------------------
class IPricingStrategy {
public:
    virtual ~IPricingStrategy() = default;
    virtual double calculateFare(double distanceKm) = 0;
};

class StandardPricingStrategy : public IPricingStrategy {
public:
    StandardPricingStrategy(double baseFare = 5.0, double perKmRate = 1.5)
        : baseFare_(baseFare), perKmRate_(perKmRate) {}

    double calculateFare(double distanceKm) override {
        return baseFare_ + (distanceKm * perKmRate_);
    }

private:
    double baseFare_;
    double perKmRate_;
};

class SurgePricingStrategy : public IPricingStrategy {
public:
    SurgePricingStrategy(double surgeMultiplier, double baseFare = 5.0, double perKmRate = 1.5)
        : multiplier_(surgeMultiplier), baseFare_(baseFare), perKmRate_(perKmRate) {}

    double calculateFare(double distanceKm) override {
        return (baseFare_ + (distanceKm * perKmRate_)) * multiplier_;
    }

private:
    double multiplier_;
    double baseFare_;
    double perKmRate_;
};

// -------------------------------------------------------------
// Ride Model
// -------------------------------------------------------------
class Ride {
public:
    Ride(string id, shared_ptr<Rider> rider, Location pickup, Location dropoff, double estimatedFare)
        : id_(move(id)), rider_(move(rider)), pickupLocation_(pickup),
          dropoffLocation_(dropoff), fare_(estimatedFare), status_(RideStatus::REQUESTED) {}

    const string& getId() const { return id_; }
    shared_ptr<Rider> getRider() const { return rider_; }
    shared_ptr<Driver> getDriver() const { return driver_; }
    Location getPickup() const { return pickupLocation_; }
    Location getDropoff() const { return dropoffLocation_; }
    double getFare() const { return fare_; }
    RideStatus getStatus() const { return status_; }

    void assignDriver(shared_ptr<Driver> driver) {
        driver_ = move(driver);
        status_ = RideStatus::ACCEPTED;
    }

    void startRide() {
        if (status_ == RideStatus::ACCEPTED) {
            status_ = RideStatus::IN_PROGRESS;
        }
    }

    void completeRide() {
        if (status_ == RideStatus::IN_PROGRESS) {
            status_ = RideStatus::COMPLETED;
            if (driver_) {
                driver_->setLocation(dropoffLocation_);
                driver_->setAvailable(true);
            }
        }
    }

    void cancelRide() {
        status_ = RideStatus::CANCELLED;
        if (driver_) {
            driver_->setAvailable(true);
        }
    }

private:
    string id_;
    shared_ptr<Rider> rider_;
    shared_ptr<Driver> driver_;
    Location pickupLocation_;
    Location dropoffLocation_;
    double fare_;
    RideStatus status_;
};

// -------------------------------------------------------------
// Strategy Pattern: Driver Matching
// -------------------------------------------------------------
class IDriverMatchingStrategy {
public:
    virtual ~IDriverMatchingStrategy() = default;
    virtual shared_ptr<Driver> matchDriver(const Location& pickup, const vector<shared_ptr<Driver>>& drivers) = 0;
};

class NearestDriverMatchingStrategy : public IDriverMatchingStrategy {
public:
    shared_ptr<Driver> matchDriver(const Location& pickup, const vector<shared_ptr<Driver>>& drivers) override {
        shared_ptr<Driver> nearestDriver = nullptr;
        double minDistance = numeric_limits<double>::max();

        for (const auto& driver : drivers) {
            if (driver->isAvailable()) {
                double dist = driver->getLocation().distanceTo(pickup);
                if (dist < minDistance) {
                    minDistance = dist;
                    nearestDriver = driver;
                }
            }
        }
        return nearestDriver;
    }
};

// -------------------------------------------------------------
// Ride-Sharing Service Orchestrator
// -------------------------------------------------------------
class RideService {
public:
    RideService()
        : matchingStrategy_(make_unique<NearestDriverMatchingStrategy>()),
          pricingStrategy_(make_unique<StandardPricingStrategy>()),
          rideCounter_(1) {}

    void registerRider(const string& id, const string& name, const string& phone) {
        riders_[id] = make_shared<Rider>(id, name, phone);
    }

    void registerDriver(const string& id, const string& name, Location location) {
        drivers_[id] = make_shared<Driver>(id, name, location);
    }

    void setPricingStrategy(unique_ptr<IPricingStrategy> strategy) {
        pricingStrategy_ = move(strategy);
    }

    shared_ptr<Ride> requestRide(const string& riderId, Location pickup, Location dropoff) {
        auto riderIt = riders_.find(riderId);
        if (riderIt == riders_.end()) {
            cout << "Rider not found: " << riderId << "\n";
            return nullptr;
        }

        double distance = pickup.distanceTo(dropoff);
        double fare = pricingStrategy_->calculateFare(distance);

        string rideId = "ride_" + to_string(rideCounter_++);
        auto ride = make_shared<Ride>(rideId, riderIt->second, pickup, dropoff, fare);

        cout << "\n[RIDE REQUESTED] ID: " << rideId << " for " << riderIt->second->getName()
             << " | Est. Distance: " << fixed << setprecision(2) << distance << " km"
             << " | Est. Fare: $" << fare << "\n";

        // Find available driver
        vector<shared_ptr<Driver>> driverList;
        for (const auto& [_, driver] : drivers_) {
            driverList.push_back(driver);
        }

        auto matchedDriver = matchingStrategy_->matchDriver(pickup, driverList);
        if (!matchedDriver) {
            cout << "[NO DRIVERS AVAILABLE] Ride " << rideId << " could not be matched.\n";
            ride->cancelRide();
            return ride;
        }

        matchedDriver->setAvailable(false);
        ride->assignDriver(matchedDriver);
        rides_[rideId] = ride;

        cout << "[DRIVER ASSIGNED] Driver " << matchedDriver->getName()
             << " (Rating: " << setprecision(1) << matchedDriver->getAverageRating() << "★)"
             << " assigned to " << riderIt->second->getName() << "\n";

        return ride;
    }

    void startRide(const string& rideId) {
        auto it = rides_.find(rideId);
        if (it != rides_.end()) {
            it->second->startRide();
            cout << "[RIDE STARTED] Ride " << rideId << " is now in progress.\n";
        }
    }

    void completeRide(const string& rideId, int driverRating = 5) {
        auto it = rides_.find(rideId);
        if (it != rides_.end()) {
            auto ride = it->second;
            ride->completeRide();
            if (ride->getDriver()) {
                ride->getDriver()->addRating(driverRating);
            }
            cout << "[RIDE COMPLETED] Ride " << rideId
                 << " completed! Fare paid: $" << fixed << setprecision(2) << ride->getFare()
                 << " | Driver rated: " << driverRating << "★\n";
        }
    }

private:
    unordered_map<string, shared_ptr<Rider>> riders_;
    unordered_map<string, shared_ptr<Driver>> drivers_;
    unordered_map<string, shared_ptr<Ride>> rides_;
    unique_ptr<IDriverMatchingStrategy> matchingStrategy_;
    unique_ptr<IPricingStrategy> pricingStrategy_;
    int rideCounter_;
};

// -------------------------------------------------------------
// Driver Demo
// -------------------------------------------------------------
int main() {
    RideService uber;

    cout << "====================================================\n";
    cout << "     Ride-Sharing Service (Uber / Lyft) - LLD       \n";
    cout << "====================================================\n\n";

    // 1. Setup Riders & Drivers
    uber.registerRider("r1", "Alice", "123-456-7890");
    uber.registerRider("r2", "Bob", "987-654-3210");

    uber.registerDriver("d1", "Dave (Downtown)", Location(10.0, 10.0));
    uber.registerDriver("d2", "Dan (Uptown)", Location(50.0, 50.0));

    // 2. Standard Ride Request
    cout << "--- Test 1: Standard Ride Matching & Completion ---\n";
    auto ride1 = uber.requestRide("r1", Location(11.0, 10.5), Location(20.0, 20.0));
    if (ride1 && ride1->getStatus() == RideStatus::ACCEPTED) {
        uber.startRide(ride1->getId());
        uber.completeRide(ride1->getId(), 5);
    }

    // 3. Surge Pricing Scenario
    cout << "\n--- Test 2: Surge Pricing Ride ---\n";
    cout << "Applying 2.0x Peak Hour Surge Multiplier...\n";
    uber.setPricingStrategy(make_unique<SurgePricingStrategy>(2.0));

    auto ride2 = uber.requestRide("r2", Location(48.0, 49.0), Location(60.0, 60.0));
    if (ride2 && ride2->getStatus() == RideStatus::ACCEPTED) {
        uber.startRide(ride2->getId());
        uber.completeRide(ride2->getId(), 4);
    }

    return 0;
}
