/*
 * Elevator System - interview-style low-level design
 *
 * Question:
 * Design an elevator system for a building with multiple floors and elevators.
 * The system should accept requests from inside and outside an elevator,
 * assign a suitable elevator, and move it to each requested floor.
 *
 * The example keeps dispatching and elevator state separate so that the
 * selection strategy can be replaced without changing the Elevator class.
 */

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

enum class Direction {
    Down = -1,
    Idle = 0,
    Up = 1
};

class Elevator {
public:
    Elevator(int id, int initialFloor) : id_(id), currentFloor_(initialFloor) {}

    int id() const {
        return id_;
    }

    int currentFloor() const {
        return currentFloor_;
    }

    Direction direction() const {
        if (stops_.empty()) {
            return Direction::Idle;
        }

        return stops_.front() >= currentFloor_ ? Direction::Up : Direction::Down;
    }

    bool isAvailableFor(int floor, Direction requestedDirection) const {
        if (stops_.empty()) {
            return true;
        }

        const Direction currentDirection = direction();
        return currentDirection == requestedDirection &&
               (requestedDirection == Direction::Up
                    ? floor >= currentFloor_
                    : floor <= currentFloor_);
    }

    void addStop(int floor) {
        if (find(stops_.begin(), stops_.end(), floor) == stops_.end()) {
            stops_.push_back(floor);
            sort(stops_.begin(), stops_.end());

            if (direction() == Direction::Down) {
                reverse(stops_.begin(), stops_.end());
            }
        }
    }

    void moveUntilIdle() {
        while (!stops_.empty()) {
            const int destination = stops_.front();
            cout << "Elevator " << id_ << " moving from floor " << currentFloor_
                 << " to floor " << destination << ".\n";
            currentFloor_ = destination;
            stops_.erase(stops_.begin());
            cout << "Elevator " << id_ << " arrived at floor " << currentFloor_
                 << ".\n";
        }
    }

private:
    int id_;
    int currentFloor_;
    vector<int> stops_;
};

class ElevatorSystem {
public:
    ElevatorSystem(int floorCount, int elevatorCount) {
        if (floorCount <= 0 || elevatorCount <= 0) {
            throw invalid_argument("Floor and elevator counts must be positive.");
        }

        for (int id = 1; id <= elevatorCount; ++id) {
            elevators_.emplace_back(id, 0);
        }
        maxFloor_ = floorCount - 1;
    }

    void requestElevator(int floor, Direction requestedDirection) {
        validateFloor(floor);

        Elevator* selected = selectElevator(floor, requestedDirection);
        if (selected == nullptr) {
            cout << "No suitable elevator is currently available.\n";
            return;
        }

        selected->addStop(floor);
        cout << "Elevator " << selected->id() << " assigned to floor " << floor
             << ".\n";
    }

    void selectFloor(int elevatorId, int floor) {
        validateFloor(floor);

        for (Elevator& elevator : elevators_) {
            if (elevator.id() == elevatorId) {
                elevator.addStop(floor);
                return;
            }
        }

        throw invalid_argument("Elevator ID was not found.");
    }

    void run() {
        for (Elevator& elevator : elevators_) {
            elevator.moveUntilIdle();
        }
    }

private:
    Elevator* selectElevator(int floor, Direction requestedDirection) {
        Elevator* closest = nullptr;
        int closestDistance = numeric_limits<int>::max();

        for (Elevator& elevator : elevators_) {
            if (!elevator.isAvailableFor(floor, requestedDirection)) {
                continue;
            }

            const int distance = abs(elevator.currentFloor() - floor);
            if (distance < closestDistance) {
                closest = &elevator;
                closestDistance = distance;
            }
        }

        return closest;
    }

    void validateFloor(int floor) const {
        if (floor < 0 || floor > maxFloor_) {
            throw out_of_range("Requested floor is outside the building.");
        }
    }

    int maxFloor_;
    vector<Elevator> elevators_;
};

int main() {
    try {
        ElevatorSystem system(10, 2);

        system.requestElevator(5, Direction::Up);
        system.requestElevator(8, Direction::Up);
        system.selectFloor(1, 2);
        system.run();
    } catch (const exception& error) {
        cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
