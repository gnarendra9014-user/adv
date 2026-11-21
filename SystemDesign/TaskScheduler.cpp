/*
 * Task Scheduler / Cron Job Executor - Interview-Style Low-Level Design (LLD)
 *
 * Question:
 * Design an in-memory Task Scheduler (similar to Cron, ThreadPoolExecutor, or Quartz) that supports:
 *   1. Scheduling one-time delayed tasks (run at timestamp / delay).
 *   2. Scheduling recurring periodic tasks (fixed interval).
 *   3. Priority and dynamic earliest-deadline scheduling using a Min-Heap (Priority Queue).
 *   4. Multi-worker execution simulation with task lifecycle states:
 *      SCHEDULED -> RUNNING -> COMPLETED / CANCELLED.
 *   5. Task cancellation: Cancel a scheduled task before it executes.
 */

#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <chrono>
#include <functional>
#include <memory>
#include <unordered_map>
#include <thread>

using namespace std;
using namespace std::chrono;

// -------------------------------------------------------------
// Enums & Models
// -------------------------------------------------------------
enum class TaskStatus {
    SCHEDULED,
    RUNNING,
    COMPLETED,
    CANCELLED
};

class ScheduledTask {
public:
    ScheduledTask(string id, string name, system_clock::time_point executionTime,
                  function<void()> action, milliseconds interval = milliseconds(0))
        : id_(move(id)), name_(move(name)), executionTime_(executionTime),
          action_(move(action)), interval_(interval), status_(TaskStatus::SCHEDULED) {}

    const string& getId() const { return id_; }
    const string& getName() const { return name_; }
    system_clock::time_point getExecutionTime() const { return executionTime_; }
    milliseconds getInterval() const { return interval_; }
    bool isRecurring() const { return interval_.count() > 0; }
    TaskStatus getStatus() const { return status_; }

    void setStatus(TaskStatus s) { status_ = s; }
    void rescheduleNext() {
        executionTime_ += interval_;
        status_ = TaskStatus::SCHEDULED;
    }

    void execute() {
        if (status_ == TaskStatus::CANCELLED) return;
        status_ = TaskStatus::RUNNING;
        cout << "[EXECUTING] Task ID: " << id_ << " (" << name_ << ")\n";
        action_();
        if (isRecurring()) {
            rescheduleNext();
            cout << "  ↳ Rescheduled recurring task " << id_ << " for next interval.\n";
        } else {
            status_ = TaskStatus::COMPLETED;
        }
    }

private:
    string id_;
    string name_;
    system_clock::time_point executionTime_;
    function<void()> action_;
    milliseconds interval_;
    TaskStatus status_;
};

// Comparator for Priority Queue (Min-Heap based on earliest execution time)
struct TaskComparator {
    bool operator()(const shared_ptr<ScheduledTask>& a, const shared_ptr<ScheduledTask>& b) const {
        return a->getExecutionTime() > b->getExecutionTime();
    }
};

// -------------------------------------------------------------
// Task Scheduler Service
// -------------------------------------------------------------
class TaskScheduler {
public:
    TaskScheduler() : isRunning_(true) {}

    // Schedule a one-time task with delay
    string schedule(const string& name, milliseconds delay, function<void()> action) {
        auto runAt = system_clock::now() + delay;
        string taskId = "task_" + to_string(++idCounter_);
        auto task = make_shared<ScheduledTask>(taskId, name, runAt, move(action));

        taskMap_[taskId] = task;
        taskQueue_.push(task);

        cout << "[SCHEDULED] " << name << " (ID: " << taskId
             << ") in " << delay.count() << " ms.\n";
        return taskId;
    }

    // Schedule a recurring periodic task
    string scheduleAtFixedRate(const string& name, milliseconds initialDelay,
                               milliseconds interval, function<void()> action) {
        auto runAt = system_clock::now() + initialDelay;
        string taskId = "task_" + to_string(++idCounter_);
        auto task = make_shared<ScheduledTask>(taskId, name, runAt, move(action), interval);

        taskMap_[taskId] = task;
        taskQueue_.push(task);

        cout << "[SCHEDULED RECURRING] " << name << " (ID: " << taskId
             << ") initial delay: " << initialDelay.count()
             << " ms, period: " << interval.count() << " ms.\n";
        return taskId;
    }

    // Cancel a scheduled task
    bool cancel(const string& taskId) {
        auto it = taskMap_.find(taskId);
        if (it == taskMap_.end() || it->second->getStatus() == TaskStatus::COMPLETED) {
            cout << "[CANCEL FAILED] Task " << taskId << " not found or already completed.\n";
            return false;
        }
        it->second->setStatus(TaskStatus::CANCELLED);
        cout << "[CANCELLED] Task " << taskId << " has been cancelled.\n";
        return true;
    }

    // Poll and run due tasks (simulating scheduler tick)
    void tick() {
        auto now = system_clock::now();

        while (!taskQueue_.empty()) {
            auto topTask = taskQueue_.top();

            if (topTask->getStatus() == TaskStatus::CANCELLED) {
                taskQueue_.pop();
                continue;
            }

            if (topTask->getExecutionTime() <= now) {
                taskQueue_.pop();
                topTask->execute();

                // If recurring and still active, push back to queue
                if (topTask->isRecurring() && topTask->getStatus() == TaskStatus::SCHEDULED) {
                    taskQueue_.push(topTask);
                }
            } else {
                // Earliest task is in the future
                break;
            }
        }
    }

    size_t pendingTaskCount() const {
        return taskQueue_.size();
    }

    // Prints status, remaining time, and type of all registered tasks
    void printTaskStatus() const {
        cout << "\n--- Task Scheduler Status Overview ---\n";
        if (taskMap_.empty()) {
            cout << "  No registered tasks.\n";
            cout << "--------------------------------------\n";
            return;
        }

        auto now = system_clock::now();
        for (const auto& [id, task] : taskMap_) {
            string statusStr;
            switch (task->getStatus()) {
                case TaskStatus::SCHEDULED: statusStr = "SCHEDULED"; break;
                case TaskStatus::RUNNING:   statusStr = "RUNNING";   break;
                case TaskStatus::COMPLETED: statusStr = "COMPLETED"; break;
                case TaskStatus::CANCELLED: statusStr = "CANCELLED"; break;
            }

            cout << "• [" << id << "] " << task->getName()
                 << " | Status: " << statusStr
                 << " | Type: " << (task->isRecurring() ? "RECURRING" : "ONE-TIME");

            if (task->getStatus() == TaskStatus::SCHEDULED) {
                auto diff = duration_cast<milliseconds>(task->getExecutionTime() - now).count();
                if (diff > 0) {
                    cout << " | Next run in: " << diff << " ms";
                } else {
                    cout << " | Due now!";
                }
            }
            cout << "\n";
        }
        cout << "--------------------------------------\n";
    }

private:
    priority_queue<shared_ptr<ScheduledTask>, vector<shared_ptr<ScheduledTask>>, TaskComparator> taskQueue_;
    unordered_map<string, shared_ptr<ScheduledTask>> taskMap_;
    int idCounter_ = 0;
    bool isRunning_;
};

// -------------------------------------------------------------
// Driver Demo
// -------------------------------------------------------------
int main() {
    TaskScheduler scheduler;

    cout << "====================================================\n";
    cout << "        Task Scheduler / Cron Engine (LLD)          \n";
    cout << "====================================================\n\n";

    // 1. One-time task with delay
    cout << "--- 1. Schedule One-Time Tasks ---\n";
    scheduler.schedule("Send Welcome Email", milliseconds(100), []() {
        cout << "   -> Sending welcome email to user...\n";
    });

    scheduler.schedule("Generate Analytics Report", milliseconds(300), []() {
        cout << "   -> Generating daily report...\n";
    });

    // 2. Periodic recurring task
    cout << "\n--- 2. Schedule Recurring Task ---\n";
    string heartBeatId = scheduler.scheduleAtFixedRate("Heartbeat Ping", milliseconds(50), milliseconds(100), []() {
        cout << "   -> Heartbeat ping OK.\n";
    });

    // 3. Task cancellation demonstration
    cout << "\n--- 3. Schedule and Cancel a Task ---\n";
    string cancelledTaskId = scheduler.schedule("Cleanup Temp Files", milliseconds(200), []() {
        cout << "   -> This should not execute!\n";
    });
    scheduler.cancel(cancelledTaskId);

    // 4. Simulate Scheduler Execution loop across ~450ms
    cout << "\n--- 4. Running Scheduler Loop (450ms simulation) ---\n";
    for (int step = 0; step < 9; ++step) {
        scheduler.tick();
        this_thread::sleep_for(milliseconds(50));
    }

    // Cancel recurring task
    cout << "\nCancelling recurring heartbeat task...\n";
    scheduler.cancel(heartBeatId);

    // 5. Inspect final task registry statuses
    scheduler.printTaskStatus();

    return 0;
}
