#include "../src/spmc_queue.h"
#include <gtest/gtest.h>
#include <thread>
#include <cstring>
#include <mutex>

// Test case for a single producer and a single consumer.
// It enqueues data and ensures it can be dequeued correctly.
TEST(SPMCQueueTest, SingleProducerSingleConsumer) {
    SPMCQueue queue(10);

    uint8_t data[64];
    std::memset(data, 42, sizeof(data));

    bool result = queue.enqueue(data, sizeof(data));
    EXPECT_TRUE(result);

    uint8_t buffer[64];
    size_t size;

    result = queue.dequeue(buffer, size);
    EXPECT_TRUE(result);
    EXPECT_EQ(size, sizeof(data));
    EXPECT_EQ(buffer[0], 42); // Ensure the first byte matches the original data
}

// Test case for enqueueing when the queue is full.
// The queue should allow enqueueing more data by overwriting older entries.
TEST(SPMCQueueTest, EnqueueWhenFull) {
    SPMCQueue queue(2);

    uint8_t data[64];
    std::memset(data, 42, sizeof(data));

    // Enqueue data multiple times
    EXPECT_TRUE(queue.enqueue(data, sizeof(data)));
    EXPECT_TRUE(queue.enqueue(data, sizeof(data)));
    EXPECT_TRUE(queue.enqueue(data, sizeof(data)));
}

// Test case for dequeueing from an empty queue.
// The dequeue operation should fail when the queue is empty.
TEST(SPMCQueueTest, DequeueWhenEmpty) {
    SPMCQueue queue(10);

    uint8_t buffer[64];
    size_t size;

    EXPECT_FALSE(queue.dequeue(buffer, size));
}

// Test case for multiple consumers dequeueing from the queue.
// Ensures each consumer retrieves consecutive entries correctly.
TEST(SPMCQueueTest, MultipleConsumers) {
    SPMCQueue queue(10);
    uint8_t data[64], data2[64];
    std::memset(data, 42, sizeof(data));
    std::memset(data2, 100, sizeof(data2));

    EXPECT_TRUE(queue.enqueue(data, sizeof(data)));
    EXPECT_TRUE(queue.enqueue(data2, sizeof(data2)));

    uint8_t buffer1[64], buffer2[64];
    size_t size1, size2;

    EXPECT_TRUE(queue.dequeue(buffer1, size1));
    EXPECT_TRUE(queue.dequeue(buffer2, size2));

    EXPECT_EQ(size1, size2);
    EXPECT_EQ(buffer1[0], 42);
    EXPECT_EQ(buffer2[0], 100);
    EXPECT_NE(buffer1, buffer2);
}

// Test case for multiple producers and consumers.
// Ensures multiple threads can enqueue and dequeue data concurrently.
TEST(SPMCQueueTest, MultiProducerMultiConsumer) {
    SPMCQueue queue(10);

    auto producer = [&queue]() {
        uint8_t data[64];
        std::memset(data, 42, sizeof(data));
        for (int i = 0; i < 5; ++i) {
            while (!queue.enqueue(data, sizeof(data))) {
                std::this_thread::yield();
            }
        }
    };

    auto consumer = [&queue](uint8_t* buffer, size_t& size) {
        for (int i = 0; i < 5; ++i) {
            while (!queue.dequeue(buffer, size)) {
                std::this_thread::yield();
            }
        }
    };

    uint8_t buffer1[64], buffer2[64];
    size_t size1, size2;

    std::thread producer1(producer);
    std::thread producer2(producer);
    std::thread consumer1(consumer, buffer1, std::ref(size1));
    std::thread consumer2(consumer, buffer2, std::ref(size2));

    producer1.join();
    producer2.join();
    consumer1.join();
    consumer2.join();

    EXPECT_EQ(buffer1[0], 42);
    EXPECT_EQ(buffer2[0], 42);
}

// Global counter for consumer tests
int counter = 0;
std::mutex mtx;

// Function to increment the global counter
void increment_counter(int value) {
    std::lock_guard<std::mutex> lock(mtx);
    counter += value;
}

// Test case for a single producer with multiple consumers.
// Each consumer reads from the queue and increments a global counter.
TEST(SPMCQueueTest, SingleProducerMultipleConsumers) {
    SPMCQueue queue(20);

    // enqueues data with values from 1 to 20
    auto producer = [&queue]() {
        for (int i = 1; i <= 20; ++i) {
            uint8_t data[64];
            std::memset(data, i, sizeof(data)); // Fill with value `i`
            while (!queue.enqueue(data, sizeof(data))) {
                std::this_thread::yield(); // Retry if enqueue fails
            }
        }
    };

    auto consumer = [&queue]() {
        for (int i = 0; i < 20 / 2; ++i) {
            uint8_t buffer[64];
            size_t size = 0;
            while (!queue.dequeue(buffer, size)) {
                std::this_thread::yield();
            }
            increment_counter(buffer[0]);
        }
    };

    std::thread producerThread(producer);
    std::thread consumerThread1(consumer);
    std::thread consumerThread2(consumer);

    producerThread.join();
    consumerThread1.join();
    consumerThread2.join();

    int expectedSum = 0;
    for (int i = 1; i <= 20; ++i) {
        expectedSum += i;
    }

    std::cout << counter << std::endl;
    EXPECT_EQ(counter, expectedSum);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
