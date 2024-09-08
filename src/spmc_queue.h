#ifndef SPMC_QUEUE_H
#define SPMC_QUEUE_H

#include <atomic>
#include <cstdint>
#include <iostream>

struct Block {
    std::atomic<size_t> mVersion;  // Local block version
    std::atomic<size_t> mSize;     // Size of the data
    alignas(64) uint8_t mData[64]; // Data buffer (64 bytes)
};

class SPMCQueue {
public:
    SPMCQueue(size_t capacity);
    ~SPMCQueue();

    bool enqueue(const uint8_t* data, size_t size);

    bool dequeue(uint8_t* buffer, size_t& size);

private:
    size_t mCapacity;
    std::atomic<size_t> mHead;
    std::atomic<size_t> mTail;
    Block* mQueue;
};

#endif
