/*
 * Movie Ticket Booking System (BookMyShow / Fandango / AMC)
 * Low-Level Design (LLD)
 *
 * Question:
 * Design a movie ticket booking system like BookMyShow where:
 * 1. The system operates across multiple cities. Each city has multiple
 * cinemas/theaters.
 * 2. Each cinema has multiple screens (auditoriums), and each screen runs
 * multiple shows.
 * 3. A show maps a movie to a screen at a specific time slot.
 * 4. Seats are categorized by tiers (e.g., SILVER, GOLD, PLATINUM) with
 * differential pricing.
 * 5. Users can browse movies by city, select a cinema, choose a show, and view
 * seat availability.
 * 6. Concurrency & Seat Locking:
 *    - When a user selects seats, the system places a temporary lock (e.g.,
 * 5-10 minutes TTL).
 *    - Other users cannot book or lock the same seats while the lock is active.
 *    - If payment succeeds, the seats are marked as BOOKED and a booking is
 * confirmed.
 *    - If payment fails, times out, or the user cancels, the lock is released
 * back to AVAILABLE.
 * 7. Support pluggable payment methods via Strategy Pattern (UPI, Credit Card,
 * Net Banking).
 * 8. Support booking cancellation and refunds with seat status rollback.
 */

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace std;
using namespace std::chrono;

// ============================================================================
// Enums & Constants
// ============================================================================

enum class SeatType { SILVER, GOLD, PLATINUM };

string seatTypeToString(SeatType type) {
  switch (type) {
  case SeatType::SILVER:
    return "Silver";
  case SeatType::GOLD:
    return "Gold";
  case SeatType::PLATINUM:
    return "Platinum";
  }
  return "Unknown";
}

enum class SeatStatus { AVAILABLE, LOCKED, BOOKED };

string seatStatusToString(SeatStatus status) {
  switch (status) {
  case SeatStatus::AVAILABLE:
    return "AVAILABLE";
  case SeatStatus::LOCKED:
    return "LOCKED";
  case SeatStatus::BOOKED:
    return "BOOKED";
  }
  return "UNKNOWN";
}

enum class BookingStatus { PENDING_PAYMENT, CONFIRMED, EXPIRED, CANCELLED };

string bookingStatusToString(BookingStatus status) {
  switch (status) {
  case BookingStatus::PENDING_PAYMENT:
    return "Pending Payment";
  case BookingStatus::CONFIRMED:
    return "Confirmed";
  case BookingStatus::EXPIRED:
    return "Expired";
  case BookingStatus::CANCELLED:
    return "Cancelled";
  }
  return "Unknown";
}

// ============================================================================
// Core Domain Models
// ============================================================================

class User {
public:
  User(string id, string name, string email, string phone)
      : id_(move(id)), name_(move(name)), email_(move(email)),
        phone_(move(phone)) {}

  const string &getId() const { return id_; }
  const string &getName() const { return name_; }
  const string &getEmail() const { return email_; }

private:
  string id_;
  string name_;
  string email_;
  string phone_;
};

class Movie {
public:
  Movie(string id, string title, string language, string genre,
        int durationMinutes)
      : id_(move(id)), title_(move(title)), language_(move(language)),
        genre_(move(genre)), durationMinutes_(durationMinutes) {}

  const string &getId() const { return id_; }
  const string &getTitle() const { return title_; }
  const string &getLanguage() const { return language_; }
  const string &getGenre() const { return genre_; }
  int getDurationMinutes() const { return durationMinutes_; }

private:
  string id_;
  string title_;
  string language_;
  string genre_;
  int durationMinutes_;
};

class Seat {
public:
  Seat(string id, int row, int col, SeatType type, double basePrice)
      : id_(move(id)), row_(row), col_(col), type_(type),
        basePrice_(basePrice) {}

  const string &getId() const { return id_; }
  int getRow() const { return row_; }
  int getCol() const { return col_; }
  SeatType getType() const { return type_; }
  double getBasePrice() const { return basePrice_; }

private:
  string id_;
  int row_;
  int col_;
  SeatType type_;
  double basePrice_;
};

class Screen {
public:
  Screen(string id, string name, vector<Seat> seats)
      : id_(move(id)), name_(move(name)), seats_(move(seats)) {}

  const string &getId() const { return id_; }
  const string &getName() const { return name_; }
  const vector<Seat> &getSeats() const { return seats_; }

  const Seat *getSeat(const string &seatId) const {
    for (const auto &seat : seats_) {
      if (seat.getId() == seatId)
        return &seat;
    }
    return nullptr;
  }

private:
  string id_;
  string name_;
  vector<Seat> seats_;
};

class Cinema {
public:
  Cinema(string id, string name, string city, string address)
      : id_(move(id)), name_(move(name)), city_(move(city)),
        address_(move(address)) {}

  void addScreen(shared_ptr<Screen> screen) { screens_.push_back(screen); }

  const string &getId() const { return id_; }
  const string &getName() const { return name_; }
  const string &getCity() const { return city_; }
  const string &getAddress() const { return address_; }
  const vector<shared_ptr<Screen>> &getScreens() const { return screens_; }

private:
  string id_;
  string name_;
  string city_;
  string address_;
  vector<shared_ptr<Screen>> screens_;
};

// Represents a scheduled movie screening in a specific screen of a cinema
class Show {
public:
  Show(string id, shared_ptr<Movie> movie, shared_ptr<Screen> screen,
       shared_ptr<Cinema> cinema, string startTime, string endTime)
      : id_(move(id)), movie_(move(movie)), screen_(move(screen)),
        cinema_(move(cinema)), startTime_(move(startTime)),
        endTime_(move(endTime)) {
    // Initialize seat statuses to AVAILABLE
    for (const auto &seat : screen_->getSeats()) {
      seatStatusMap_[seat.getId()] = SeatStatus::AVAILABLE;
    }
  }

  const string &getId() const { return id_; }
  shared_ptr<Movie> getMovie() const { return movie_; }
  shared_ptr<Screen> getScreen() const { return screen_; }
  shared_ptr<Cinema> getCinema() const { return cinema_; }
  const string &getStartTime() const { return startTime_; }
  const string &getEndTime() const { return endTime_; }

  SeatStatus getSeatStatus(const string &seatId) const {
    auto it = seatStatusMap_.find(seatId);
    if (it != seatStatusMap_.end())
      return it->second;
    return SeatStatus::AVAILABLE;
  }

  void setSeatStatus(const string &seatId, SeatStatus status) {
    seatStatusMap_[seatId] = status;
  }

  const unordered_map<string, SeatStatus> &getAllSeatStatuses() const {
    return seatStatusMap_;
  }

private:
  string id_;
  shared_ptr<Movie> movie_;
  shared_ptr<Screen> screen_;
  shared_ptr<Cinema> cinema_;
  string startTime_;
  string endTime_;
  unordered_map<string, SeatStatus> seatStatusMap_;
};

// ============================================================================
// Seat Locking Mechanism (Handles Concurrency & Timeouts)
// ============================================================================

struct SeatLock {
  string lockId;
  string showId;
  string seatId;
  string userId;
  steady_clock::time_point lockTime;
  seconds timeoutDuration;

  bool isExpired() const {
    return duration_cast<seconds>(steady_clock::now() - lockTime) >=
           timeoutDuration;
  }
};

class SeatLockService {
public:
  explicit SeatLockService(seconds defaultTimeout = seconds(300))
      : defaultTimeout_(defaultTimeout) {}

  // Attempt to lock a list of seats for a given user and show atomically
  bool lockSeats(const shared_ptr<Show> &show, const vector<string> &seatIds,
                 const string &userId, seconds customTimeout = seconds(0)) {
    lock_guard<mutex> guard(mutex_);

    // 0. Clean up any expired locks for this show before checking availability
    pruneExpiredLocksInternal(show);

    // 1. Validate if all requested seats are currently lockable
    for (const auto &seatId : seatIds) {
      if (isSeatLockedInternal(show->getId(), seatId)) {
        return false;
      }
      if (show->getSeatStatus(seatId) != SeatStatus::AVAILABLE) {
        return false;
      }
    }

    // 2. Acquire lock on all requested seats
    seconds timeout =
        (customTimeout.count() > 0) ? customTimeout : defaultTimeout_;
    auto now = steady_clock::now();

    for (const auto &seatId : seatIds) {
      string key = makeKey(show->getId(), seatId);
      SeatLock lock{key, show->getId(), seatId, userId, now, timeout};
      locks_[key] = lock;
      show->setSeatStatus(seatId, SeatStatus::LOCKED);
    }

    return true;
  }

  // Release locks (e.g. after payment failure, cancellation, or manual
  // unreserve)
  void unlockSeats(const shared_ptr<Show> &show, const vector<string> &seatIds,
                   const string &userId) {
    lock_guard<mutex> guard(mutex_);

    for (const auto &seatId : seatIds) {
      string key = makeKey(show->getId(), seatId);
      auto it = locks_.find(key);
      if (it != locks_.end() && it->second.userId == userId) {
        locks_.erase(it);
        if (show->getSeatStatus(seatId) == SeatStatus::LOCKED) {
          show->setSeatStatus(seatId, SeatStatus::AVAILABLE);
        }
      }
    }
  }

  // Verify if seats are locked by this user and lock is still valid
  bool validateUserLocks(const string &showId, const vector<string> &seatIds,
                         const string &userId) {
    lock_guard<mutex> guard(mutex_);

    for (const auto &seatId : seatIds) {
      string key = makeKey(showId, seatId);
      auto it = locks_.find(key);
      if (it == locks_.end())
        return false;
      if (it->second.userId != userId)
        return false;
      if (it->second.isExpired()) {
        // Lazily cleanup expired lock
        locks_.erase(it);
        return false;
      }
    }
    return true;
  }

  // Check and prune expired locks across a show
  void pruneExpiredLocks(const shared_ptr<Show> &show) {
    lock_guard<mutex> guard(mutex_);
    pruneExpiredLocksInternal(show);
  }

  // Permanently remove locks after successful booking conversion
  void finalizeSeats(const shared_ptr<Show> &show,
                     const vector<string> &seatIds) {
    lock_guard<mutex> guard(mutex_);

    for (const auto &seatId : seatIds) {
      string key = makeKey(show->getId(), seatId);
      locks_.erase(key);
      show->setSeatStatus(seatId, SeatStatus::BOOKED);
    }
  }

private:
  void pruneExpiredLocksInternal(const shared_ptr<Show> &show) {
    for (auto it = locks_.begin(); it != locks_.end();) {
      if (it->second.showId == show->getId() && it->second.isExpired()) {
        if (show->getSeatStatus(it->second.seatId) == SeatStatus::LOCKED) {
          show->setSeatStatus(it->second.seatId, SeatStatus::AVAILABLE);
        }
        it = locks_.erase(it);
      } else {
        ++it;
      }
    }
  }

  string makeKey(const string &showId, const string &seatId) const {
    return showId + "_" + seatId;
  }

  bool isSeatLockedInternal(const string &showId, const string &seatId) {
    string key = makeKey(showId, seatId);
    auto it = locks_.find(key);
    if (it == locks_.end())
      return false;

    if (it->second.isExpired()) {
      locks_.erase(it);
      return false;
    }
    return true;
  }

  mutex mutex_;
  seconds defaultTimeout_;
  unordered_map<string, SeatLock> locks_; // key -> SeatLock
};

// ============================================================================
// Payment System (Strategy Pattern)
// ============================================================================

class PaymentStrategy {
public:
  virtual ~PaymentStrategy() = default;
  virtual bool processPayment(double amount, const string &bookingId) = 0;
  virtual string getPaymentMethod() const = 0;
};

class UpiPaymentStrategy : public PaymentStrategy {
public:
  explicit UpiPaymentStrategy(string upiId) : upiId_(move(upiId)) {}

  bool processPayment(double amount, const string &bookingId) override {
    cout << "  [Payment Gateway] Processing UPI payment of Rs. " << fixed
         << setprecision(2) << amount << " via UPI ID: " << upiId_
         << " for Booking ID: " << bookingId << "...\n";
    cout << "  [Payment Gateway] Payment Authorized & Settled successfully.\n";
    return true;
  }

  string getPaymentMethod() const override { return "UPI (" + upiId_ + ")"; }

private:
  string upiId_;
};

class CreditCardPaymentStrategy : public PaymentStrategy {
public:
  CreditCardPaymentStrategy(string cardNumber, string expiry)
      : cardNumber_(move(cardNumber)), expiry_(move(expiry)) {}

  bool processPayment(double amount, const string &bookingId) override {
    string maskedCard =
        "XXXX-XXXX-XXXX-" + cardNumber_.substr(cardNumber_.length() >= 4
                                                   ? cardNumber_.length() - 4
                                                   : 0);
    cout << "  [Payment Gateway] Processing Credit Card payment of Rs. "
         << fixed << setprecision(2) << amount << " on Card " << maskedCard
         << " for Booking ID: " << bookingId << "...\n";
    cout
        << "  [Payment Gateway] OTP validated. Charge captured successfully.\n";
    return true;
  }

  string getPaymentMethod() const override { return "Credit Card"; }

private:
  string cardNumber_;
  string expiry_;
};

// ============================================================================
// Booking Model
// ============================================================================

class Booking {
public:
  Booking(string id, shared_ptr<User> user, shared_ptr<Show> show,
          vector<Seat> seats, double totalAmount)
      : id_(move(id)), user_(move(user)), show_(move(show)),
        seats_(move(seats)), totalAmount_(totalAmount),
        status_(BookingStatus::PENDING_PAYMENT),
        createdAt_(system_clock::to_time_t(system_clock::now())) {}

  const string &getId() const { return id_; }
  shared_ptr<User> getUser() const { return user_; }
  shared_ptr<Show> getShow() const { return show_; }
  const vector<Seat> &getSeats() const { return seats_; }
  double getTotalAmount() const { return totalAmount_; }
  BookingStatus getStatus() const { return status_; }
  void setStatus(BookingStatus status) { status_ = status; }
  void setPaymentMethod(string method) { paymentMethod_ = move(method); }

  void printTicket() const {
    cout << "\n======================================================\n";
    cout << "                 MOVIE TICKET / BOOKING PASS          \n";
    cout << "======================================================\n";
    cout << " Booking ID     : " << id_ << "\n";
    cout << " Status         : " << bookingStatusToString(status_) << "\n";
    cout << " Customer Name  : " << user_->getName() << " ("
         << user_->getEmail() << ")\n";
    cout << " Movie          : " << show_->getMovie()->getTitle() << " ["
         << show_->getMovie()->getLanguage() << " | "
         << show_->getMovie()->getGenre() << "]\n";
    cout << " Cinema         : " << show_->getCinema()->getName() << ", "
         << show_->getCinema()->getCity() << "\n";
    cout << " Screen         : " << show_->getScreen()->getName() << "\n";
    cout << " Show Time      : " << show_->getStartTime() << " - "
         << show_->getEndTime() << "\n";
    cout << " Seats Reserved : ";
    for (size_t i = 0; i < seats_.size(); ++i) {
      cout << seats_[i].getId() << " (" << seatTypeToString(seats_[i].getType())
           << ")";
      if (i + 1 < seats_.size())
        cout << ", ";
    }
    cout << "\n";
    cout << " Payment Method : "
         << (paymentMethod_.empty() ? "N/A" : paymentMethod_) << "\n";
    cout << " Total Paid     : Rs. " << fixed << setprecision(2) << totalAmount_
         << "\n";
    cout << "======================================================\n\n";
  }

private:
  string id_;
  shared_ptr<User> user_;
  shared_ptr<Show> show_;
  vector<Seat> seats_;
  double totalAmount_;
  BookingStatus status_;
  string paymentMethod_;
  time_t createdAt_;
};

// ============================================================================
// Movie Booking Service (Facade / Coordinator)
// ============================================================================

class MovieBookingService {
public:
  explicit MovieBookingService(shared_ptr<SeatLockService> seatLockService)
      : seatLockService_(move(seatLockService)), bookingCounter_(1000) {}

  // Catalog & Management APIs
  void addCity(string city) { cities_.push_back(move(city)); }

  void addCinema(shared_ptr<Cinema> cinema) { cinemas_.push_back(cinema); }

  void addMovie(shared_ptr<Movie> movie) { movies_.push_back(movie); }

  void addShow(shared_ptr<Show> show) { shows_.push_back(show); }

  // Search movies currently showing in a given city
  vector<shared_ptr<Movie>> searchMoviesByCity(const string &city) const {
    vector<shared_ptr<Movie>> result;
    unordered_map<string, bool> seen;

    for (const auto &show : shows_) {
      if (show->getCinema()->getCity() == city) {
        if (!seen[show->getMovie()->getId()]) {
          seen[show->getMovie()->getId()] = true;
          result.push_back(show->getMovie());
        }
      }
    }
    return result;
  }

  // Get all shows for a particular movie in a city
  vector<shared_ptr<Show>> getShows(const string &movieId,
                                    const string &city) const {
    vector<shared_ptr<Show>> result;
    for (const auto &show : shows_) {
      if (show->getMovie()->getId() == movieId &&
          show->getCinema()->getCity() == city) {
        result.push_back(show);
      }
    }
    return result;
  }

  // Display seat layout with live statuses for a show
  void displaySeatLayout(const shared_ptr<Show> &show) {
    seatLockService_->pruneExpiredLocks(show);

    cout << "\n--- Seat Map for: " << show->getMovie()->getTitle() << " at "
         << show->getCinema()->getName() << " (" << show->getScreen()->getName()
         << ") ---\n";
    cout << "Screen This Way: [ __________________ SCREEN __________________ "
            "]\n\n";

    const auto &seats = show->getScreen()->getSeats();
    int currentRow = -1;

    for (const auto &seat : seats) {
      if (seat.getRow() != currentRow) {
        if (currentRow != -1)
          cout << "\n";
        currentRow = seat.getRow();
        cout << "Row " << setw(2) << currentRow << " | ";
      }

      SeatStatus status = show->getSeatStatus(seat.getId());
      char symbol = 'O'; // Available
      if (status == SeatStatus::LOCKED)
        symbol = 'L';
      else if (status == SeatStatus::BOOKED)
        symbol = 'X';

      cout << "[" << seat.getId() << ":" << symbol << ":"
           << seatTypeToString(seat.getType())[0] << "] ";
    }
    cout << "\n\nLegend: [O] Available  |  [L] Temporarily Locked  |  [X] "
            "Booked\n";
    cout << "        Seat Tiers: S = Silver, G = Gold, P = Platinum\n\n";
  }

  // Step 1: Temporarily lock selected seats
  bool selectAndLockSeats(const shared_ptr<Show> &show,
                          const vector<string> &seatIds,
                          const shared_ptr<User> &user,
                          seconds customTimeout = seconds(0)) {
    bool locked = seatLockService_->lockSeats(show, seatIds, user->getId(),
                                              customTimeout);
    if (locked) {
      cout << ">> [Lock Success] User " << user->getName()
           << " acquired temporary hold on " << seatIds.size() << " seat(s).\n";
    } else {
      cout << ">> [Lock Failed] Could not lock requested seat(s) for "
           << user->getName()
           << ". One or more seats are unavailable or held by another "
              "customer.\n";
    }
    return locked;
  }

  // Step 2: Create a pending booking for locked seats
  shared_ptr<Booking> createBooking(const shared_ptr<Show> &show,
                                    const vector<string> &seatIds,
                                    const shared_ptr<User> &user) {
    if (!seatLockService_->validateUserLocks(show->getId(), seatIds,
                                             user->getId())) {
      cout << ">> [Booking Failed] Seat lock has expired or is invalid for "
              "user: "
           << user->getName() << "\n";
      return nullptr;
    }

    vector<Seat> selectedSeats;
    double totalAmount = 0.0;

    for (const auto &id : seatIds) {
      const Seat *s = show->getScreen()->getSeat(id);
      if (s) {
        selectedSeats.push_back(*s);
        totalAmount += s->getBasePrice();
      }
    }

    string bookingId = "BKG-" + to_string(++bookingCounter_);
    auto booking =
        make_shared<Booking>(bookingId, user, show, selectedSeats, totalAmount);
    bookings_[bookingId] = booking;

    cout << ">> [Booking Created] Booking ID " << bookingId
         << " initialized. Total Amount: Rs. " << fixed << setprecision(2)
         << totalAmount << ". Awaiting payment.\n";
    return booking;
  }

  // Step 3: Confirm booking with payment
  bool confirmBooking(const shared_ptr<Booking> &booking,
                      unique_ptr<PaymentStrategy> paymentStrategy) {
    if (!booking)
      return false;

    const auto &show = booking->getShow();
    vector<string> seatIds;
    for (const auto &s : booking->getSeats()) {
      seatIds.push_back(s.getId());
    }

    // Validate that lock is still active right before charging payment
    if (!seatLockService_->validateUserLocks(show->getId(), seatIds,
                                             booking->getUser()->getId())) {
      cout << ">> [Payment Aborted] Seat hold expired before checkout "
              "completion. Lock released.\n";
      booking->setStatus(BookingStatus::EXPIRED);
      return false;
    }

    // Execute payment strategy
    bool paymentSuccess = paymentStrategy->processPayment(
        booking->getTotalAmount(), booking->getId());

    if (paymentSuccess) {
      seatLockService_->finalizeSeats(show, seatIds);
      booking->setStatus(BookingStatus::CONFIRMED);
      booking->setPaymentMethod(paymentStrategy->getPaymentMethod());
      cout << ">> [Booking Confirmed] Booking " << booking->getId()
           << " successfully confirmed!\n";
      return true;
    } else {
      cout << ">> [Payment Failed] Payment failed. Releasing locked seats.\n";
      seatLockService_->unlockSeats(show, seatIds, booking->getUser()->getId());
      booking->setStatus(BookingStatus::CANCELLED);
      return false;
    }
  }

  // Step 4: Cancel a confirmed booking
  bool cancelBooking(const string &bookingId) {
    auto it = bookings_.find(bookingId);
    if (it == bookings_.end()) {
      cout << ">> Booking ID " << bookingId << " not found.\n";
      return false;
    }

    auto booking = it->second;
    if (booking->getStatus() != BookingStatus::CONFIRMED) {
      cout << ">> Only confirmed bookings can be cancelled.\n";
      return false;
    }

    const auto &show = booking->getShow();
    for (const auto &seat : booking->getSeats()) {
      show->setSeatStatus(seat.getId(), SeatStatus::AVAILABLE);
    }

    booking->setStatus(BookingStatus::CANCELLED);
    cout << ">> [Booking Cancelled] Booking " << bookingId
         << " has been cancelled. Refund of Rs. " << fixed << setprecision(2)
         << booking->getTotalAmount() << " initiated.\n";
    return true;
  }

  // Prune locks
  void pruneLocks(const shared_ptr<Show> &show) {
    seatLockService_->pruneExpiredLocks(show);
  }

private:
  shared_ptr<SeatLockService> seatLockService_;
  vector<string> cities_;
  vector<shared_ptr<Cinema>> cinemas_;
  vector<shared_ptr<Movie>> movies_;
  vector<shared_ptr<Show>> shows_;
  unordered_map<string, shared_ptr<Booking>> bookings_;
  int bookingCounter_;
};

// ============================================================================
// Demonstration & Interview Walkthrough
// ============================================================================

int main() {
  cout << "===============================================================\n";
  cout << "  Movie Ticket Booking System (BookMyShow) - LLD Demo         \n";
  cout << "===============================================================\n\n";

  // 1. Initialize Seat Lock Service (default timeout 5 seconds for demo)
  auto lockService = make_shared<SeatLockService>(seconds(5));
  MovieBookingService bms(lockService);

  // 2. Setup Master Data (Cities, Cinemas, Screens, Movies)
  bms.addCity("Bangalore");
  bms.addCity("Mumbai");

  auto movie1 = make_shared<Movie>("M1", "Inception", "English",
                                   "Sci-Fi / Thriller", 148);
  auto movie2 = make_shared<Movie>("M2", "Interstellar", "English",
                                   "Sci-Fi / Drama", 169);
  bms.addMovie(movie1);
  bms.addMovie(movie2);

  auto cinema1 = make_shared<Cinema>("C1", "PVR Forum Mall", "Bangalore",
                                     "Koramangala, Bangalore");
  auto cinema2 = make_shared<Cinema>("C2", "INOX Garuda", "Bangalore",
                                     "Magrath Road, Bangalore");
  bms.addCinema(cinema1);
  bms.addCinema(cinema2);

  // Create seats for screen (2 rows, 4 seats each)
  vector<Seat> screen1Seats = {Seat("A1", 1, 1, SeatType::SILVER, 180.00),
                               Seat("A2", 1, 2, SeatType::SILVER, 180.00),
                               Seat("A3", 1, 3, SeatType::GOLD, 250.00),
                               Seat("A4", 1, 4, SeatType::GOLD, 250.00),
                               Seat("B1", 2, 1, SeatType::PLATINUM, 350.00),
                               Seat("B2", 2, 2, SeatType::PLATINUM, 350.00),
                               Seat("B3", 2, 3, SeatType::PLATINUM, 350.00),
                               Seat("B4", 2, 4, SeatType::PLATINUM, 350.00)};

  auto screen1 =
      make_shared<Screen>("SCR-1", "Audi 1 (Dolby Atmos)", screen1Seats);
  cinema1->addScreen(screen1);

  // Create a Show
  auto eveningShow =
      make_shared<Show>("SHOW-101", movie1, screen1, cinema1, "18:30", "21:00");
  bms.addShow(eveningShow);

  // 3. User Registration
  auto alice = make_shared<User>("U1", "Alice Sharma", "alice@example.com",
                                 "+91-9876543210");
  auto bob =
      make_shared<User>("U2", "Bob Verma", "bob@example.com", "+91-9876543211");
  auto charlie = make_shared<User>("U3", "Charlie Pinto", "charlie@example.com",
                                   "+91-9876543212");

  // ------------------------------------------------------------------------
  // Scenario 1: Search & Browse Movies in Bangalore
  // ------------------------------------------------------------------------
  cout << "[TEST 1] Browsing movies in Bangalore:\n";
  auto blrMovies = bms.searchMoviesByCity("Bangalore");
  for (const auto &m : blrMovies) {
    cout << "  - " << m->getTitle() << " (" << m->getLanguage() << ", "
         << m->getGenre() << ")\n";
  }

  cout << "\n[TEST 2] Getting shows for 'Inception' in Bangalore:\n";
  auto shows = bms.getShows(movie1->getId(), "Bangalore");
  for (const auto &s : shows) {
    cout << "  - Show ID: " << s->getId() << " at " << s->getCinema()->getName()
         << " [" << s->getStartTime() << "]\n";
  }

  // Display initial seat layout
  bms.displaySeatLayout(eveningShow);

  // ------------------------------------------------------------------------
  // Scenario 2: Successful Booking Flow by Alice
  // ------------------------------------------------------------------------
  cout << "\n[TEST 3] Alice selects seats A3 (Gold) and A4 (Gold)...\n";
  bool aliceLocked = bms.selectAndLockSeats(eveningShow, {"A3", "A4"}, alice);
  bms.displaySeatLayout(eveningShow);

  if (aliceLocked) {
    auto aliceBooking = bms.createBooking(eveningShow, {"A3", "A4"}, alice);
    // Alice pays using UPI
    auto upiPayment = make_unique<UpiPaymentStrategy>("alice@oksbi");
    bms.confirmBooking(aliceBooking, move(upiPayment));
    aliceBooking->printTicket();
  }

  bms.displaySeatLayout(eveningShow);

  // ------------------------------------------------------------------------
  // Scenario 3: Race Condition / Conflict Handling (Bob tries to book A3)
  // ------------------------------------------------------------------------
  cout << "\n[TEST 4] Bob tries to book already BOOKED seat A3 and AVAILABLE "
          "B1...\n";
  bool bobLocked = bms.selectAndLockSeats(eveningShow, {"A3", "B1"}, bob);
  cout << "  Bob's lock attempt outcome: "
       << (bobLocked ? "SUCCESS" : "FAILED (as expected)") << "\n";

  // ------------------------------------------------------------------------
  // Scenario 4: Temporary Seat Lock & Expiration / Timeout Handling
  // ------------------------------------------------------------------------
  cout << "\n[TEST 5] Charlie selects seats B1 & B2 with a short 2-second lock "
          "timeout...\n";
  bool charlieLocked =
      bms.selectAndLockSeats(eveningShow, {"B1", "B2"}, charlie, seconds(2));
  cout << "  Charlie lock status: " << (charlieLocked ? "ACQUIRED" : "FAILED")
       << "\n";
  bms.displaySeatLayout(eveningShow);

  cout << "  Bob tries to lock B1 while Charlie holds the lock...\n";
  bool bobRetry = bms.selectAndLockSeats(eveningShow, {"B1"}, bob);
  cout << "  Bob's attempt: "
       << (bobRetry ? "SUCCESS" : "FAILED (Charlie has lock)") << "\n";

  cout << "\n  [Simulating user inactivity] Waiting 3 seconds for Charlie's "
          "lock to expire...\n";
  std::this_thread::sleep_for(std::chrono::seconds(3));

  cout << "  Now Bob retries booking seat B1 after Charlie's lock expired...\n";
  bool bobAfterExpiry = bms.selectAndLockSeats(eveningShow, {"B1"}, bob);
  cout << "  Bob's attempt after expiry: "
       << (bobAfterExpiry ? "SUCCESS (Lock acquired)" : "FAILED") << "\n";
  bms.displaySeatLayout(eveningShow);

  if (bobAfterExpiry) {
    auto bobBooking = bms.createBooking(eveningShow, {"B1"}, bob);
    auto ccPayment =
        make_unique<CreditCardPaymentStrategy>("4111222233334444", "12/28");
    bms.confirmBooking(bobBooking, move(ccPayment));
    bobBooking->printTicket();
  }

  // ------------------------------------------------------------------------
  // Scenario 5: Cancellation & Seat Re-availability
  // ------------------------------------------------------------------------
  cout << "\n[TEST 6] Bob cancels his booking BKG-1002...\n";
  bms.cancelBooking("BKG-1002");
  bms.displaySeatLayout(eveningShow);

  cout << "===============================================================\n";
  cout << "  All Test Scenarios Completed Successfully!                   \n";
  cout << "===============================================================\n";

  return 0;
}
