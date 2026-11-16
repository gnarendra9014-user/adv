/*
 * URL Shortener (TinyURL) - Interview-Style Low-Level Design (LLD)
 *
 * Question:
 * Design a URL shortening service (like TinyURL or Bitly) with:
 *   1. Encoding: Shorten a long URL to a unique short key (e.g., Base62 string).
 *   2. Decoding/Redirection: Given a short URL or key, retrieve the original URL and redirect.
 *   3. Custom Aliases: Allow users to specify optional custom short aliases.
 *   4. TTL / Expiration: Set an optional expiration time on URLs.
 *   5. Analytics: Track access counts for each shortened URL.
 *   6. Collision Handling / Uniqueness: Using Base62 counter encoding and hash fallbacks.
 */

#include <iostream>
#include <string>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <algorithm>

using namespace std;
using namespace std::chrono;

// -------------------------------------------------------------
// Base62 Converter Utility
// -------------------------------------------------------------
class Base62 {
private:
    static constexpr const char* ALPHABET =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

public:
    static string encode(unsigned long long id) {
        if (id == 0) return "a";

        string shortUrl;
        while (id > 0) {
            shortUrl.push_back(ALPHABET[id % 62]);
            id /= 62;
        }
        reverse(shortUrl.begin(), shortUrl.end());
        return shortUrl;
    }
};

// -------------------------------------------------------------
// Model: ShortURL Record
// -------------------------------------------------------------
struct URLRecord {
    string shortKey;
    string longUrl;
    string customAlias;
    system_clock::time_point createdAt;
    system_clock::time_point expiresAt;
    bool hasExpiry;
    long long clickCount;

    URLRecord(string key, string url, bool hasExp = false, system_clock::time_point expTime = system_clock::time_point())
        : shortKey(move(key)),
          longUrl(move(url)),
          createdAt(system_clock::now()),
          expiresAt(expTime),
          hasExpiry(hasExp),
          clickCount(0) {}

    bool isExpired() const {
        if (!hasExpiry) return false;
        return system_clock::now() > expiresAt;
    }
};

// -------------------------------------------------------------
// URL Shortener Service
// -------------------------------------------------------------
class URLShortenerService {
public:
    explicit URLShortenerService(string domain = "https://tiny.url/")
        : baseDomain_(move(domain)), counter_(100000000ULL) {} // Start counter to ensure fixed minimum key length

    // Shorten URL with auto-generated short key
    string shorten(const string& longUrl, int ttlSeconds = 0) {
        // Check if long URL is already mapped and active
        auto it = longToKeyMap_.find(longUrl);
        if (it != longToKeyMap_.end()) {
            const auto& existingRecord = keyToRecordMap_[it->second];
            if (!existingRecord->isExpired()) {
                return baseDomain_ + existingRecord->shortKey;
            }
        }

        string shortKey = Base62::encode(counter_++);

        bool hasExp = (ttlSeconds > 0);
        system_clock::time_point expTime = hasExp ? (system_clock::now() + seconds(ttlSeconds)) : system_clock::time_point();

        auto record = make_shared<URLRecord>(shortKey, longUrl, hasExp, expTime);
        keyToRecordMap_[shortKey] = record;
        longToKeyMap_[longUrl] = shortKey;

        return baseDomain_ + shortKey;
    }

    // Shorten URL with a custom alias
    bool shortenWithCustomAlias(const string& longUrl, const string& customAlias, string& resultUrl, int ttlSeconds = 0) {
        if (customAlias.empty()) {
            cout << "Custom alias cannot be empty.\n";
            return false;
        }

        auto it = keyToRecordMap_.find(customAlias);
        if (it != keyToRecordMap_.end() && !it->second->isExpired()) {
            cout << "Custom alias '" << customAlias << "' is already in use.\n";
            return false;
        }

        bool hasExp = (ttlSeconds > 0);
        system_clock::time_point expTime = hasExp ? (system_clock::now() + seconds(ttlSeconds)) : system_clock::time_point();

        auto record = make_shared<URLRecord>(customAlias, longUrl, hasExp, expTime);
        record->customAlias = customAlias;
        keyToRecordMap_[customAlias] = record;
        longToKeyMap_[longUrl] = customAlias;

        resultUrl = baseDomain_ + customAlias;
        return true;
    }

    // Resolve short key to the original long URL (simulates 301/302 redirect)
    string resolve(const string& shortKeyOrUrl) {
        string key = extractKey(shortKeyOrUrl);

        auto it = keyToRecordMap_.find(key);
        if (it == keyToRecordMap_.end()) {
            cout << "[404] Short key '" << key << "' not found.\n";
            return "";
        }

        auto& record = it->second;
        if (record->isExpired()) {
            cout << "[410 Gone] Short URL for key '" << key << "' has expired.\n";
            return "";
        }

        // Increment analytics counter
        record->clickCount++;
        return record->longUrl;
    }

    // Get analytics for a shortened URL
    void getAnalytics(const string& shortKeyOrUrl) const {
        string key = extractKey(shortKeyOrUrl);
        auto it = keyToRecordMap_.find(key);

        if (it == keyToRecordMap_.end()) {
            cout << "No record found for key: " << key << "\n";
            return;
        }

        const auto& record = it->second;
        cout << "\n--- Analytics for [" << key << "] ---\n";
        cout << "Original URL : " << record->longUrl << "\n";
        cout << "Short URL    : " << baseDomain_ + key << "\n";
        cout << "Clicks       : " << record->clickCount << "\n";
        cout << "Expired      : " << (record->isExpired() ? "Yes" : "No") << "\n";
        cout << "------------------------------------\n";
    }

private:
    string extractKey(const string& input) const {
        if (input.rfind(baseDomain_, 0) == 0) {
            return input.substr(baseDomain_.length());
        }
        return input;
    }

    string baseDomain_;
    unsigned long long counter_;
    unordered_map<string, shared_ptr<URLRecord>> keyToRecordMap_;
    unordered_map<string, string> longToKeyMap_;
};

// -------------------------------------------------------------
// Driver Demo
// -------------------------------------------------------------
int main() {
    URLShortenerService service("https://tiny.url/");

    cout << "====================================================\n";
    cout << "        URL Shortener (TinyURL) - System Design     \n";
    cout << "====================================================\n\n";

    // 1. Basic auto-generated short URL
    cout << "--- Test 1: Basic URL Shortening ---\n";
    string longUrl1 = "https://www.example.com/articles/system-design-lld-guide-2026";
    string shortUrl1 = service.shorten(longUrl1);
    cout << "Long URL  : " << longUrl1 << "\n";
    cout << "Short URL : " << shortUrl1 << "\n";

    // Resolve / Redirect
    cout << "\nResolving Short URL:\n";
    string redirectedUrl1 = service.resolve(shortUrl1);
    cout << "Redirects to: " << redirectedUrl1 << "\n";

    // 2. Custom Alias
    cout << "\n--- Test 2: Custom Alias ---\n";
    string longUrl2 = "https://github.com/gnarendra9014-user/adv";
    string customUrl;
    if (service.shortenWithCustomAlias(longUrl2, "my-repo", customUrl)) {
        cout << "Custom Short URL created: " << customUrl << "\n";
    }

    // Attempt duplicate custom alias
    string duplicateCheckUrl;
    service.shortenWithCustomAlias("https://google.com", "my-repo", duplicateCheckUrl);

    // 3. Analytics simulation
    cout << "\n--- Test 3: Analytics / Click Tracking ---\n";
    service.resolve("my-repo");
    service.resolve("my-repo");
    service.resolve("my-repo");
    service.getAnalytics("my-repo");

    // 4. URL with expiration (TTL)
    cout << "\n--- Test 4: TTL / Expiration Demo ---\n";
    string tempLongUrl = "https://example.com/one-time-secret-token";
    string tempShortUrl = service.shorten(tempLongUrl, 1); // 1 second TTL
    cout << "Short URL with 1s TTL: " << tempShortUrl << "\n";

    cout << "Immediate resolution: " << service.resolve(tempShortUrl) << "\n";

    return 0;
}
