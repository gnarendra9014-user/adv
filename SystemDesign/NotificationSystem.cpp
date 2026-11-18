/*
 * Notification System - Interview-Style Low-Level Design (LLD)
 *
 * Question:
 * Design a scalable multi-channel Notification Service that can:
 *   1. Support multiple delivery channels: Email, SMS, Push Notification.
 *   2. Support multiple notification priority levels: HIGH, MEDIUM, LOW.
 *   3. Decouple notification dispatching using the Observer / Pub-Sub pattern.
 *   4. Channel-specific gateways using the Strategy / Factory Pattern.
 *   5. User notification preferences (e.g., opting out of SMS or marketing channels).
 *   6. Deduplication and rate limiting / logging per user.
 */

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <chrono>

using namespace std;

// -------------------------------------------------------------
// Enums & Models
// -------------------------------------------------------------
enum class ChannelType {
    EMAIL,
    SMS,
    PUSH
};

enum class Priority {
    LOW,
    MEDIUM,
    HIGH
};

struct Notification {
    string id;
    string recipientId;
    string title;
    string body;
    Priority priority;
    ChannelType channel;

    Notification(string id, string recipientId, string title, string body, Priority prio, ChannelType ch)
        : id(move(id)), recipientId(move(recipientId)), title(move(title)),
          body(move(body)), priority(prio), channel(ch) {}
};

struct UserProfile {
    string userId;
    string name;
    string email;
    string phone;
    string deviceToken;
    unordered_set<ChannelType> enabledChannels;

    UserProfile() = default;

    UserProfile(string id, string name, string email, string phone, string deviceToken)
        : userId(move(id)), name(move(name)), email(move(email)),
          phone(move(phone)), deviceToken(move(deviceToken)) {
        // By default, enable all channels
        enabledChannels.insert(ChannelType::EMAIL);
        enabledChannels.insert(ChannelType::SMS);
        enabledChannels.insert(ChannelType::PUSH);
    }
};

// -------------------------------------------------------------
// Channel Strategy: Notification Sender Interface
// -------------------------------------------------------------
class INotificationSender {
public:
    virtual ~INotificationSender() = default;
    virtual bool send(const Notification& notification, const UserProfile& user) = 0;
};

class EmailSender : public INotificationSender {
public:
    bool send(const Notification& notification, const UserProfile& user) override {
        if (user.email.empty()) {
            cout << "[EMAIL FAILED] No email found for user " << user.name << "\n";
            return false;
        }
        cout << "[EMAIL SENT] To: " << user.email
             << " | Subject: [" << notification.title << "] | Message: " << notification.body << "\n";
        return true;
    }
};

class SMSSender : public INotificationSender {
public:
    bool send(const Notification& notification, const UserProfile& user) override {
        if (user.phone.empty()) {
            cout << "[SMS FAILED] No phone number for user " << user.name << "\n";
            return false;
        }
        cout << "[SMS SENT] To: " << user.phone
             << " | Body: " << notification.body << "\n";
        return true;
    }
};

class PushNotificationSender : public INotificationSender {
public:
    bool send(const Notification& notification, const UserProfile& user) override {
        if (user.deviceToken.empty()) {
            cout << "[PUSH FAILED] No device token for user " << user.name << "\n";
            return false;
        }
        cout << "[PUSH SENT] Device: " << user.deviceToken
             << " | Title: " << notification.title
             << " | Body: " << notification.body << "\n";
        return true;
    }
};

// -------------------------------------------------------------
// Notification Gateway / Service Context
// -------------------------------------------------------------
class NotificationService {
public:
    NotificationService() {
        senders_[ChannelType::EMAIL] = make_shared<EmailSender>();
        senders_[ChannelType::SMS] = make_shared<SMSSender>();
        senders_[ChannelType::PUSH] = make_shared<PushNotificationSender>();
    }

    void registerUser(const UserProfile& user) {
        users_[user.userId] = user;
    }

    void setUserChannelPreference(const string& userId, ChannelType channel, bool enable) {
        auto it = users_.find(userId);
        if (it != users_.end()) {
            if (enable) {
                it->second.enabledChannels.insert(channel);
            } else {
                it->second.enabledChannels.erase(channel);
            }
        }
    }

    bool dispatch(const Notification& notification) {
        auto it = users_.find(notification.recipientId);
        if (it == users_.end()) {
            cout << "[DISPATCH FAILED] Unknown user ID: " << notification.recipientId << "\n";
            return false;
        }

        const auto& user = it->second;

        // Check user preferences
        if (user.enabledChannels.find(notification.channel) == user.enabledChannels.end()) {
            cout << "[OPT-OUT] User " << user.name << " has opted out of "
                 << channelToString(notification.channel) << " notifications.\n";
            return false;
        }

        auto senderIt = senders_.find(notification.channel);
        if (senderIt == senders_.end()) {
            cout << "[ERROR] Sender not supported for this channel.\n";
            return false;
        }

        bool success = senderIt->second->send(notification, user);
        if (success) {
            notificationLog_.push_back(notification);
        }
        return success;
    }

    void sendBroadcast(const string& title, const string& body, ChannelType channel, Priority priority) {
        cout << "\n--- Broadcasting to all users via " << channelToString(channel) << " ---\n";
        int count = 1;
        for (const auto& [id, user] : users_) {
            Notification notif("broadcast_" + to_string(count++), id, title, body, priority, channel);
            dispatch(notif);
        }
    }

    size_t getLoggedCount() const {
        return notificationLog_.size();
    }

private:
    static string channelToString(ChannelType ch) {
        switch (ch) {
            case ChannelType::EMAIL: return "EMAIL";
            case ChannelType::SMS: return "SMS";
            case ChannelType::PUSH: return "PUSH";
        }
        return "UNKNOWN";
    }

    unordered_map<string, UserProfile> users_;
    unordered_map<ChannelType, shared_ptr<INotificationSender>> senders_;
    vector<Notification> notificationLog_;
};

// -------------------------------------------------------------
// Driver Demo
// -------------------------------------------------------------
int main() {
    NotificationService service;

    cout << "====================================================\n";
    cout << "      Multi-Channel Notification System (LLD)       \n";
    cout << "====================================================\n\n";

    // Setup user profiles
    UserProfile alice("u1", "Alice", "alice@test.com", "+1234567890", "token_alice_ios");
    UserProfile bob("u2", "Bob", "bob@test.com", "+1987654321", ""); // No device token
    UserProfile charlie("u3", "Charlie", "charlie@test.com", "", "token_charlie_android"); // No phone

    service.registerUser(alice);
    service.registerUser(bob);
    service.registerUser(charlie);

    // 1. Dispatch individual notifications across channels
    cout << "--- Test 1: Direct Multi-Channel Dispatch ---\n";
    Notification n1("n1", "u1", "Security Alert", "New login detected from Chrome.", Priority::HIGH, ChannelType::EMAIL);
    service.dispatch(n1);

    Notification n2("n2", "u1", "OTP", "Your code is 492011", Priority::HIGH, ChannelType::SMS);
    service.dispatch(n2);

    Notification n3("n3", "u3", "Friend Request", "Alice sent you a friend request.", Priority::MEDIUM, ChannelType::PUSH);
    service.dispatch(n3);

    // 2. Failure due to missing channel credentials
    cout << "\n--- Test 2: Channel Credential Validation ---\n";
    Notification n4("n4", "u2", "Update", "New feature live!", Priority::LOW, ChannelType::PUSH); // Bob has no push token
    service.dispatch(n4);

    // 3. User opt-out preference
    cout << "\n--- Test 3: User Opt-Out / Preferences ---\n";
    service.setUserChannelPreference("u1", ChannelType::SMS, false); // Alice opts out of SMS
    Notification n5("n5", "u1", "Promo", "50% off pizza tonight!", Priority::LOW, ChannelType::SMS);
    service.dispatch(n5);

    // 4. System Broadcast
    cout << "\n--- Test 4: System Broadcast ---\n";
    service.sendBroadcast("System Maintenance", "Scheduled maintenance tonight at 2 AM UTC.", ChannelType::EMAIL, Priority::MEDIUM);

    cout << "\nTotal successfully delivered notifications: " << service.getLoggedCount() << "\n";

    return 0;
}
