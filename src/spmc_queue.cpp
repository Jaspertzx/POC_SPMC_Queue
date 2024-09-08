#include "spmc_queue.h"
#include <iostream>
#include <cstring>

// Constructor for SPMCQueue.
// Initializes the queue with a given capacity, setting the head and tail pointers to 0.
// Allocates memory for the queue blocks and initializes the version and size of each block.
SPMCQueue::SPMCQueue(size_t capacity) : mCapacity(capacity), mHead(0), mTail(0) {
    mQueue = new Block[capacity];
    for (size_t i = 0; i < capacity; ++i) {
        mQueue[i].mVersion.store(0);
        mQueue[i].mSize.store(0);
    }
}

// Destructor for SPMCQueue.
SPMCQueue::~SPMCQueue() {
    delete[] mQueue;
}

// Enqueue function: Adds a block of data to the queue.
// Parameters:
// - data: pointer to the data to be enqueued.
// - size: size of the data to be enqueued.
// Returns:
// - true if the data was successfully enqueued.
bool SPMCQueue::enqueue(const uint8_t* data, size_t size) {
    Block& block = mQueue[mHead % mCapacity]; // Get the block at the head position
    size_t version = block.mVersion.load(std::memory_order_acquire); // Get the current version of the block

    block.mVersion.store(1, std::memory_order_release);

    std::memcpy(block.mData, data, size);
    block.mSize.store(size, std::memory_order_release);

    block.mVersion.fetch_add(1, std::memory_order_release);

    mHead = (mHead + 1) % mCapacity;

    return true;
}

// Dequeue function: Retrieves a block of data from the queue.
// Parameters:
// - buffer: pointer to the buffer where the data will be copied.
// - size: reference to a variable to store the size of the dequeued data.
// Returns:
// - true if data was successfully dequeued, false if the block is not ready to be read.
bool SPMCQueue::dequeue(uint8_t* buffer, size_t& size) {
    size_t localTail = mTail;
    Block& block = mQueue[localTail % mCapacity];
    size_t version = block.mVersion.load(std::memory_order_acquire);

    // Check if the block is still being written to (odd version) or if it hasn't been written to yet (version == 0)
    if (version % 2 == 1 || version == 0) {
        return false; // Cannot dequeue if the block is not ready
    }

    if (!std::atomic_compare_exchange_strong(&mTail, &localTail, (localTail + 1) % mCapacity)) {
        return false;
    }

    size = block.mSize.load(std::memory_order_acquire);

    std::memcpy(buffer, block.mData, size);

    block.mVersion.fetch_add(2, std::memory_order_release);

    return true;
}
