#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <chrono>
#include <queue>
#include <cstring>
#include <atomic>
#include <memory>
#include "../src/spmc_queue.h"

class MutexQueue {
public:
    void enqueue(const uint8_t* data, size_t size) {
        std::lock_guard<std::mutex> lock(mMutex);
        std::vector<uint8_t> buffer(data, data + size);
        mQueue.push(buffer);
    }

    bool dequeue(uint8_t* buffer, size_t& size) {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mQueue.empty()) return false;

        std::vector<uint8_t> front = mQueue.front();
        mQueue.pop();
        std::memcpy(buffer, front.data(), front.size());
        size = front.size();
        return true;
    }

private:
    std::queue<std::vector<uint8_t>> mQueue;
    std::mutex mMutex;
};

template <typename QueueType>
void benchmarkQueue(QueueType& queue, int numIterations, int numProducers, int numConsumers, const std::string& queueName) {
    auto start = std::chrono::high_resolution_clock::now();

    std::atomic<bool> startFlag{false};
    std::atomic<int> completedProducers{0};
    std::atomic<uint64_t> totalEnqueueSum{0};
    std::mutex sumMutex;

    auto producer = [&](int id) {
        uint8_t data[64];
        std::memset(data, id + 1, sizeof(data));
        uint64_t producerSum = 0;

        while (!startFlag) {}

        for (int i = 0; i < numIterations; ++i) {
            queue.enqueue(data, sizeof(data));
            producerSum += (id + 1);
        }

        totalEnqueueSum += producerSum;
        ++completedProducers;
    };

    auto consumer = [&](size_t maxDataToConsume = 5000000) {
        uint8_t buffer[64];
        size_t size = 0;
        uint64_t consumerSum = 0;
        size_t dataConsumed = 0;

        while ((completedProducers.load() < numProducers || queue.dequeue(buffer, size)) && dataConsumed < maxDataToConsume) {
            if (queue.dequeue(buffer, size)) {
                ++dataConsumed;

                if (dataConsumed >= maxDataToConsume) {
                    break;
                }
            }
        }
    };

    std::vector<std::thread> producerThreads, consumerThreads;

    for (int i = 0; i < numProducers; ++i) {
        producerThreads.emplace_back(producer, i);
    }

    for (int i = 0; i < numConsumers; ++i) {
        consumerThreads.emplace_back(consumer);
    }

    startFlag = true;

    for (auto& t : producerThreads) {
        t.join();
    }

    for (auto& t : consumerThreads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << queueName << " benchmark completed in " << duration << " ms\n";
    std::cout << "Total sum of enqueued values: " << totalEnqueueSum.load() << "\n";
}

int main() {
    const int numIterations = 5000000;
    const int numProducers = 1;
    const int numConsumers = 2;

    // Benchmark SPMCQueue
    SPMCQueue spmcQueue(1000);
    benchmarkQueue(spmcQueue, numIterations, numProducers, numConsumers, "SPMCQueue");

    // Benchmark MutexQueue
    MutexQueue mutexQueue;
    benchmarkQueue(mutexQueue, numIterations, numProducers, numConsumers, "MutexQueue");

    return 0;
}
