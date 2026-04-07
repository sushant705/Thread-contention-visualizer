#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numeric>
#include <random>
#include <semaphore>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

struct ThreadStats {
    int id{};
    int iterationsCompleted{};
    int lockRetries{};
    std::chrono::microseconds totalMutexWait{0};
    std::chrono::microseconds totalSemaphoreWait{0};
    std::chrono::steady_clock::time_point lastProgress{};
};

struct Config {
    int threadCount = 8;
    int resources = 4;
    int semaphoreSlots = 2;
    int iterations = 80;
    int thinkTimeMs = 2;
    int criticalSectionMs = 3;
    int deadlockWarnMs = 200;
};

class Simulation {
public:
    explicit Simulation(Config cfg)
        : config_(cfg),
          resourceLocks_(static_cast<std::size_t>(cfg.resources)),
          ioMutex_(),
          dbSlots_(cfg.semaphoreSlots),
          stats_(static_cast<std::size_t>(cfg.threadCount)),
          activeWorkers_(cfg.threadCount),
          stopMonitor_(false),
          blockedSince_(static_cast<std::size_t>(cfg.threadCount), std::chrono::steady_clock::time_point{}) {
        for (int i = 0; i < config_.threadCount; ++i) {
            stats_[static_cast<std::size_t>(i)].id = i;
            stats_[static_cast<std::size_t>(i)].lastProgress = std::chrono::steady_clock::now();
        }
    }

    void run() {
        std::cout << "\n=== Thread Contention Visualizer ===\n"
                  << "Threads: " << config_.threadCount << ", Shared resources: " << config_.resources
                  << ", Semaphore slots: " << config_.semaphoreSlots << "\n"
                  << "Iterations per thread: " << config_.iterations << "\n\n";

        std::thread monitor(&Simulation::monitorLoop, this);

        std::vector<std::thread> workers;
        workers.reserve(static_cast<std::size_t>(config_.threadCount));
        for (int id = 0; id < config_.threadCount; ++id) {
            workers.emplace_back(&Simulation::workerLoop, this, id);
        }

        for (auto &worker : workers) {
            worker.join();
        }

        stopMonitor_.store(true);
        monitor.join();

        printSummary();
    }

private:
    void workerLoop(int id) {
        std::minstd_rand rng(static_cast<unsigned int>(id * 7919 + 17));
        std::uniform_int_distribution<int> jitter(0, 2);

        const int leftResource = id % config_.resources;
        const int rightResource = (id + 1) % config_.resources;

        auto &localStats = stats_[static_cast<std::size_t>(id)];

        for (int i = 0; i < config_.iterations; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.thinkTimeMs + jitter(rng)));

            const auto semStart = std::chrono::steady_clock::now();
            dbSlots_.acquire();
            localStats.totalSemaphoreWait +=
                std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - semStart);

            while (true) {
                auto lockStart = std::chrono::steady_clock::now();
                bool firstLocked = resourceLocks_[static_cast<std::size_t>(leftResource)].try_lock_for(1ms);
                if (!firstLocked) {
                    ++localStats.lockRetries;
                    markBlocked(id);
                    continue;
                }

                bool secondLocked = resourceLocks_[static_cast<std::size_t>(rightResource)].try_lock_for(1ms);
                if (!secondLocked) {
                    resourceLocks_[static_cast<std::size_t>(leftResource)].unlock();
                    ++localStats.lockRetries;
                    markBlocked(id);
                    continue;
                }

                localStats.totalMutexWait += std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - lockStart);

                clearBlocked(id);
                localStats.lastProgress = std::chrono::steady_clock::now();
                ++localStats.iterationsCompleted;

                std::this_thread::sleep_for(std::chrono::milliseconds(config_.criticalSectionMs + jitter(rng)));

                resourceLocks_[static_cast<std::size_t>(rightResource)].unlock();
                resourceLocks_[static_cast<std::size_t>(leftResource)].unlock();
                break;
            }

            dbSlots_.release();
        }

        --activeWorkers_;
    }

    void markBlocked(int id) {
        auto &blockedTime = blockedSince_[static_cast<std::size_t>(id)];
        if (blockedTime == std::chrono::steady_clock::time_point{}) {
            blockedTime = std::chrono::steady_clock::now();
        }
    }

    void clearBlocked(int id) { blockedSince_[static_cast<std::size_t>(id)] = std::chrono::steady_clock::time_point{}; }

    void monitorLoop() {
        while (!stopMonitor_.load()) {
            std::this_thread::sleep_for(50ms);

            int blockedThreads = 0;
            std::ostringstream blockedSet;
            auto now = std::chrono::steady_clock::now();

            for (int i = 0; i < config_.threadCount; ++i) {
                auto blockedAt = blockedSince_[static_cast<std::size_t>(i)];
                if (blockedAt != std::chrono::steady_clock::time_point{}) {
                    auto blockedDuration = std::chrono::duration_cast<std::chrono::milliseconds>(now - blockedAt).count();
                    if (blockedDuration >= config_.deadlockWarnMs) {
                        ++blockedThreads;
                        blockedSet << i << " ";
                    }
                }
            }

            if (blockedThreads > 0) {
                std::lock_guard<std::mutex> guard(ioMutex_);
                std::cout << "[Monitor] Potential deadlock/resource contention warning: " << blockedThreads
                          << " thread(s) blocked > " << config_.deadlockWarnMs << "ms. IDs: "
                          << blockedSet.str() << "\n";
            }

            if (activeWorkers_.load() == 0) {
                break;
            }
        }
    }

    void printSummary() {
        auto avgMutexWait = averageMicroseconds(stats_, true);
        auto avgSemaphoreWait = averageMicroseconds(stats_, false);
        int totalRetries = 0;
        for (const auto &s : stats_) {
            totalRetries += s.lockRetries;
        }

        std::cout << "\n=== Summary ===\n";
        std::cout << "Total lock retries (contention indicator): " << totalRetries << "\n";
        std::cout << "Average mutex wait per thread: " << avgMutexWait << " us\n";
        std::cout << "Average semaphore wait per thread: " << avgSemaphoreWait << " us\n\n";

        std::cout << std::left << std::setw(10) << "Thread"
                  << std::setw(12) << "Iterations"
                  << std::setw(12) << "Retries"
                  << std::setw(15) << "MutexWait(us)"
                  << std::setw(15) << "SemWait(us)" << "\n";

        for (const auto &s : stats_) {
            std::cout << std::left << std::setw(10) << s.id << std::setw(12) << s.iterationsCompleted
                      << std::setw(12) << s.lockRetries << std::setw(15) << s.totalMutexWait.count()
                      << std::setw(15) << s.totalSemaphoreWait.count() << "\n";
        }

        std::cout << "\nTip: increase thread count and reduce resources to amplify contention.\n";
    }

    static long long averageMicroseconds(const std::vector<ThreadStats> &stats, bool mutex) {
        if (stats.empty()) {
            return 0;
        }

        long long total = 0;
        for (const auto &s : stats) {
            total += mutex ? s.totalMutexWait.count() : s.totalSemaphoreWait.count();
        }
        return total / static_cast<long long>(stats.size());
    }

    Config config_;
    std::vector<std::timed_mutex> resourceLocks_;
    std::mutex ioMutex_;
    std::counting_semaphore<> dbSlots_;
    std::vector<ThreadStats> stats_;
    std::atomic<int> activeWorkers_;
    std::atomic<bool> stopMonitor_;
    std::vector<std::chrono::steady_clock::time_point> blockedSince_;
};

Config parseArgs(int argc, char **argv) {
    Config cfg;

    if (argc > 1) cfg.threadCount = std::stoi(argv[1]);
    if (argc > 2) cfg.resources = std::stoi(argv[2]);
    if (argc > 3) cfg.semaphoreSlots = std::stoi(argv[3]);
    if (argc > 4) cfg.iterations = std::stoi(argv[4]);

    if (cfg.threadCount <= 0 || cfg.resources <= 1 || cfg.semaphoreSlots <= 0 || cfg.iterations <= 0) {
        throw std::invalid_argument("Invalid arguments: provide positive values; resources must be > 1.");
    }

    return cfg;
}

int main(int argc, char **argv) {
    try {
        Config config = parseArgs(argc, argv);
        Simulation simulation(config);
        simulation.run();
    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n"
                  << "Usage: ./thread_contention_visualizer [threads] [resources] [semaphore_slots] [iterations]\n";
        return 1;
    }

    return 0;
}
