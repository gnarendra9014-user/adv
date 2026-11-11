/*
 * Rate Limiter - Interview-Style Low-Level Design (LLD)
 *
 * Question:
 * Design and implement an API Rate Limiter that restricts the number of
 * requests a user or client can make within a specified time window.
 *
 * Key Design Requirements:
 *   1. Support multiple rate limiting algorithms via the Strategy Pattern:
 *      - Token Bucket: Refills tokens at a steady rate up to bucket capacity.
 *      - Sliding Window Counter: Smoothes out boundary bursts by computing
 *        weighted request counts across adjacent time frames.
 *   2. Support per-client (e.g. userId or IP) quota tracking.
 *   3. Thread-safe / extensible for distributed use cases.
 *   4. Clear allow/reject request decisions with remaining quota reporting.
 */

#include <iostream>
#include <string>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <algorithm>

using namespace std;
using namespace std::chrono;

// -------------------------------------------------------------
// Strategy Interface: RateLimiterStrategy
// -------------------------------------------------------------
class RateLimiterStrategy {
public:
    virtual ~RateLimiterStrategy() = default;

    // Returns true if request is allowed, false if rate limited
    virtual bool allowRequest() = 0;

    // Remaining tokens / allowed requests
    virtual int getRemainingTokens() = 0;
};

// -------------------------------------------------------------
// Concrete Strategy 1: Token Bucket Algorithm
// -------------------------------------------------------------
class TokenBucketStrategy : public RateLimiterStrategy {
public:
    TokenBucketStrategy(int capacity, double refillRatePerSecond)
        : capacity_(capacity),
          tokens_(capacity),
          refillRate_(refillRatePerSecond),
          lastRefillTime_(steady_clock::now()) {}

    bool allowRequest() override {
        refill();

        if (tokens_ >= 1.0) {
            tokens_ -= 1.0;
            return true;
        }
        return false;
    }

    int getRemainingTokens() override {
        refill();
        return static_cast<int>(tokens_);
    }

private:
    void refill() {
        auto now = steady_clock::now();
        double elapsedSeconds = duration_cast<duration<double>>(now - lastRefillTime_).count();
        tokens_ = min(static_cast<double>(capacity_), tokens_ + (elapsedSeconds * refillRate_));
        lastRefillTime_ = now;
    }

    int capacity_;
    double tokens_;
    double refillRate_;
    steady_clock::time_point lastRefillTime_;
};

// -------------------------------------------------------------
// Concrete Strategy 2: Sliding Window Counter
// -------------------------------------------------------------
class SlidingWindowCounterStrategy : public RateLimiterStrategy {
public:
    SlidingWindowCounterStrategy(int maxRequests, milliseconds windowDuration)
        : maxRequests_(maxRequests),
          windowDuration_(windowDuration),
          currentWindowCount_(0),
          previousWindowCount_(0),
          currentWindowStart_(steady_clock::now()) {}

    bool allowRequest() override {
        updateWindows();

        auto now = steady_clock::now();
        double timeIntoCurrentWindow = duration_cast<duration<double>>(now - currentWindowStart_).count();
        double windowSeconds = duration_cast<duration<double>>(windowDuration_).count();

        double weight = (windowSeconds - timeIntoCurrentWindow) / windowSeconds;
        double estimatedRequests = (previousWindowCount_ * weight) + currentWindowCount_;

        if (estimatedRequests < maxRequests_) {
            currentWindowCount_++;
            return true;
        }
        return false;
    }

    int getRemainingTokens() override {
        updateWindows();
        auto now = steady_clock::now();
        double timeIntoCurrentWindow = duration_cast<duration<double>>(now - currentWindowStart_).count();
        double windowSeconds = duration_cast<duration<double>>(windowDuration_).count();

        double weight = (windowSeconds - timeIntoCurrentWindow) / windowSeconds;
        double estimatedRequests = (previousWindowCount_ * weight) + currentWindowCount_;
        return max(0, static_cast<int>(maxRequests_ - estimatedRequests));
    }

private:
    void updateWindows() {
        auto now = steady_clock::now();
        auto elapsed = now - currentWindowStart_;

        if (elapsed >= windowDuration_ * 2) {
            // More than 2 windows passed, reset completely
            previousWindowCount_ = 0;
            currentWindowCount_ = 0;
            currentWindowStart_ = now;
        } else if (elapsed >= windowDuration_) {
            // Moved to the next window
            previousWindowCount_ = currentWindowCount_;
            currentWindowCount_ = 0;
            currentWindowStart_ += windowDuration_;
        }
    }

    int maxRequests_;
    milliseconds windowDuration_;
    int currentWindowCount_;
    int previousWindowCount_;
    steady_clock::time_point currentWindowStart_;
};

// -------------------------------------------------------------
// RateLimiter Service (Context managing per-client buckets)
// -------------------------------------------------------------
enum class AlgorithmType {
    TOKEN_BUCKET,
    SLIDING_WINDOW
};

class RateLimiter {
public:
    RateLimiter(AlgorithmType algo, int maxCapacityOrRequests, double rateOrWindowSec)
        : algo_(algo),
          maxLimit_(maxCapacityOrRequests),
          param_(rateOrWindowSec) {}

    bool handleRequest(const string& clientId) {
        auto& limiter = getOrCreateClientLimiter(clientId);
        bool allowed = limiter->allowRequest();
        int remaining = limiter->getRemainingTokens();

        if (allowed) {
            cout << "[ALLOWED] Client: " << clientId
                 << " | Remaining Quota: " << remaining << "\n";
        } else {
            cout << "[REJECTED] Client: " << clientId
                 << " | 429 Too Many Requests (Rate limit exceeded)\n";
        }
        return allowed;
    }

    int getRemainingQuota(const string& clientId) {
        return getOrCreateClientLimiter(clientId)->getRemainingTokens();
    }

    void resetClient(const string& clientId) {
        clientLimiters_.erase(clientId);
        cout << "Reset rate limits for client: " << clientId << "\n";
    }

private:
    shared_ptr<RateLimiterStrategy>& getOrCreateClientLimiter(const string& clientId) {
        auto it = clientLimiters_.find(clientId);
        if (it == clientLimiters_.end()) {
            if (algo_ == AlgorithmType::TOKEN_BUCKET) {
                clientLimiters_[clientId] = make_shared<TokenBucketStrategy>(maxLimit_, param_);
            } else {
                clientLimiters_[clientId] = make_shared<SlidingWindowCounterStrategy>(
                    maxLimit_, milliseconds(static_cast<long long>(param_ * 1000)));
            }
        }
        return clientLimiters_[clientId];
    }

    AlgorithmType algo_;
    int maxLimit_;
    double param_;
    unordered_map<string, shared_ptr<RateLimiterStrategy>> clientLimiters_;
};

// -------------------------------------------------------------
// Driver Demo
// -------------------------------------------------------------
int main() {
    cout << "====================================================\n";
    cout << "       API Rate Limiter - System Design Demo        \n";
    cout << "====================================================\n\n";

    // Demo 1: Token Bucket (Capacity: 3 tokens, Refill: 1 token/sec)
    cout << "--- Test 1: Token Bucket Rate Limiter ---\n";
    cout << "Config: Capacity = 3, Refill = 1 token/second\n";
    RateLimiter tokenBucketLimiter(AlgorithmType::TOKEN_BUCKET, 3, 1.0);

    string clientA = "user_alice";

    // 3 rapid requests allowed, 4th rejected
    tokenBucketLimiter.handleRequest(clientA);
    tokenBucketLimiter.handleRequest(clientA);
    tokenBucketLimiter.handleRequest(clientA);
    tokenBucketLimiter.handleRequest(clientA); // Exceeds bucket capacity

    // Demo 2: Sliding Window Counter (Max 2 requests per 1-second window)
    cout << "\n--- Test 2: Sliding Window Counter Rate Limiter ---\n";
    cout << "Config: Max = 2 requests per 1-second window\n";
    RateLimiter slidingWindowLimiter(AlgorithmType::SLIDING_WINDOW, 2, 1.0);

    string clientB = "user_bob";
    string clientC = "user_charlie";

    slidingWindowLimiter.handleRequest(clientB);
    slidingWindowLimiter.handleRequest(clientB);
    slidingWindowLimiter.handleRequest(clientB); // Exceeded for Bob

    // Independent quota for Charlie
    cout << "\nChecking independent quota for another client:\n";
    slidingWindowLimiter.handleRequest(clientC);

    // Resetting Client Bob
    cout << "\n--- Resetting Client Quota ---\n";
    slidingWindowLimiter.resetClient(clientB);
    slidingWindowLimiter.handleRequest(clientB); // Allowed after reset

    return 0;
}

