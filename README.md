# Single-Producer-Multiple-Consumer (SPMC) Queue   

![circularbuffer.png](asset%2Fcircularbuffer.png)
While listening to C++con talks, I came across this concept of a purely atomic single-producer-multiple-consumer queue 
to avoid using mutex/semaphore lock.

The `spmc_queue` is a lock-free queue that allows **one producer** to write data to the queue and **multiple consumers**
to read from it concurrently. This structure is for use in multi-threaded environments, and aims to minimize the 
computational overhead by avoiding mutexes and instead using atomic operations for concurrency control.

### Disadvantages with using locks
From my understanding, the computational overhead is too high when using mutexes and locks.  
The performance overhead is generally caused from context switching as threads are paused and resume, and lock 
contention from threads attempting to acquire the same mutex over and over.  
Additionally, locks are generally not cache-friendly, as mordern CPUs use some form of cache coherence protocols 
(like MESI) to ensure all CPU cores have a consistent view of memory. 
For example, when one thread acquires a lock, the cache line that holds the lock data is invalidated in all cores, 
causing the cores to reload the cache line from main memory, introducing more latency and reducing cache efficiency.

### Why Atomic Operations are Faster (For sharing simple data)
Looking at the assembly code for both Mutexes and Atomic Operations, we can see a difference in the number of 
instructions required. Additionally, atomic operations leverage on processor support whereas locks are more dependent 
on the operating system.
Note this is just a rough interpretation of how it works in MIPS and may not be 100% goes under the hood.
#### Mutex-Based Example in MIPS Assembly:
```asm
.data
counter:    .word 0
mutex:      .word 0  # 0 means unlocked, 1 means locked

.text
# Locking the mutex
lock_mutex:
    li      $t0, 1               # Load 1 into $t0 (indicating lock)
try_lock:
    ll      $t1, mutex           # Load linked the mutex value
    bne     $t1, $zero, try_lock # If mutex is not 0 (locked), keep trying
    sc      $t0, mutex           # Store conditional 1 (lock mutex)
    beqz    $t0, try_lock        # If store conditional failed, retry
    jr      $ra                  # Return when mutex is successfully locked

# Critical section (increment counter)
increment_counter:
    lw      $t1, counter         # Load the value of counter
    addi    $t1, $t1, 1          # Increment the value
    sw      $t1, counter         # Store the incremented value back
    jr      $ra                  # Return

# Unlocking the mutex
unlock_mutex:
    li      $t0, 0               # Load 0 (unlock value)
    sw      $t0, mutex           # Store 0 to unlock the mutex
    jr      $ra                  # Return
```
#### Atomic-Based Example in MIPS Assembly:
```asm
.data
counter:    .word 0

.text
# Atomic increment operation
atomic_increment:
    li      $t0, 1               # Load 1 (increment value)
atomic_try:
    ll      $t1, counter         # Load linked the current value of counter
    add     $t1, $t1, $t0        # Increment the value
    sc      $t1, counter         # Store conditional the new value back to counter
    beqz    $t1, atomic_try      # If store failed, retry
    jr      $ra                  # Return
```

However, [mutexes have their place too](https://stackoverflow.com/questions/15056237/which-is-more-efficient-basic-mutex-lock-or-atomic-integer).

## Implementation

### Circular Buffer
I used a circular buffer (or ring buffer) to store the blocks of data.
This was implemented as an array of fixed-size blocks, and allows for one producer thread to write data, while multiple 
consumers can read it concurrently.   
Using a head and tail pointer, we can use it to point to the position in the array where the producer will write next 
and to the position in the array where consumers will read next respectively.  
When either the mHead or mTail reaches the end of the array, they wrap around to the beginning (modulo queue size) to 
enforce the circular buffer.
#### What if the reader can't catch up?
The circular buffer should be declared to a size that is marginally large that overwriting is not an issue, for e.g., 
circular buffers can be large enough to hold about N (60 - 360) seconds worth of incoming/streaming data. (If the thread
is N seconds behind the producer, then there might be a problem in the consumer thread slowing it down).

### Block object
Each element of the buffer is stored into a block object containing
- `mData`: Actual data being stored
- `mSize`: Size of data
- `mVersion`: Atomic variable used to identify the state of block (empty, being written. ready for reading, etc)

#### Why use Block object?
Did so for Cache efficiency; it is significantly improved when data that is frequently accessed together is stored 
contiguously in memory. By keeping the data (mData) and its metadata (such as mVersion and mSize) together in a Block, 
the processor can load all of these into the cache in a single cache line, or at least fewer cache lines, rather than 
fetching them from separate, potentially distant, locations in memory.
Additionally, it helps to reduce false sharing, since each thread works on their own block, and helps to reduce cache 
invalidations across threads.

## Benchmarking
![benchmark.png](asset%2Fbenchmark.png)
The `benchmarkQueue` function compares the performance of both queues. It measures the time taken to process a number of 
iterations and outputs the total time taken to complete 2 producers and 1 consumer. (Using the join() function).


## Other Uses:

This implementation is a **multicast** one, meaning it allows **one producer** to distribute data to **multiple 
consumers** concurrently. This makes it ideal for scenarios where data needs to be processed or consumed by multiple 
threads or systems without duplicating the producer's effort.

However, it can easily be adapted for a **load-balancing queue** by re-organizing the `mVersion` state. For example, 
in a load-balancing setup, the `mVersion` states could be interpreted as follows:

- **value = 0**: The block is empty. The dequeue operation returns `false` and the index does not change.
- **value = 1**: The block is being written to. The dequeue operation returns `false` and the index does not change.
- **value = 2**: The block is ready for reading. The dequeue operation returns `true`, the index increments by 1, and 
`mVersion` increments by 2.
- **value > 2**: The block has already been processed. The index increments by 1, and the dequeue operation will check 
subsequent blocks.

For the `enqueue` method, it has to wait if **value == 0** for a consumer block to read it, or set the `mVersion` state 
to 2 whenever it finishes writing.


## Key Methods:

### How to Use

The `spmc_queue` allows one producer to enqueue data into the queue, while multiple consumers can dequeue data 
concurrently.

#### Example Usage

1. **Create a Queue**

To create an instance of `spmc_queue`, you need to specify the **capacity** of the queue. This determines how many 
blocks of data the queue can hold before wrapping around.

```cpp
#include "spmc_queue.h"

int main() {
    // Create a queue with capacity for 100 blocks
    SPMCQueue queue(100);
}
```

2. **Producer: Enqueue Data**

The producer thread calls the `enqueue()` method to add data to the queue. The data is passed as a pointer to a 
`uint8_t` array, along with the size of the data in bytes.

```cpp
uint8_t data[64] = { ... };  // Data to be enqueued
queue.enqueue(data, 64);     // Enqueue the data to the queue
```

- **Parameters**:
    - `data`: A pointer to the data to be enqueued.
    - `size`: The size of the data in bytes.

- **Returns**:
    - `true` if the data was successfully enqueued.
    - `false` if the queue is full (though this should be rare, as the queue is designed to be lock-free).

3. **Consumers: Dequeue Data**

The consumers call the `dequeue()` method to read data from the queue. A buffer is passed to hold the dequeued data, 
along with a reference to a variable to store the size of the dequeued data.

```cpp
uint8_t buffer[64];
size_t size;

if (queue.dequeue(buffer, size)) {
    // Successfully dequeued, process the data in buffer
} else {
    // No data available for reading
}
```

- **Parameters**:
    - `buffer`: A pointer to a buffer where the dequeued data will be stored.
    - `size`: A reference to a size variable where the size of the dequeued data will be stored.

- **Returns**:
    - `true` if data was successfully dequeued.
    - `false` if no data is available (e.g., the queue is empty or another consumer has already dequeued the block).

#### Thread-Safety

- **Enqueueing**: Only one producer thread can enqueue data at a time. This is handled internally by the `mHead` 
pointer, ensuring that the producer writes to the correct position in the circular buffer.
- **Dequeueing**: Multiple consumers can dequeue data concurrently, with atomic operations ensuring that only one 
consumer reads from a given block at a time. The `mTail` pointer manages the position for each consumer thread.

#### Example of Multi-Threaded Usage

```cpp
#include <thread>
#include "spmc_queue.h"

SPMCQueue queue(100);  // Queue with capacity for 100 blocks

void producer() {
    uint8_t data[64] = { /* fill with data */ };
    for (int i = 0; i < 1000; ++i) {
        queue.enqueue(data, sizeof(data));
    }
}

void consumer() {
    uint8_t buffer[64];
    size_t size;
    while (queue.dequeue(buffer, size)) {
        // Process the dequeued data
    }
}

int main() {
    // Launch producer thread
    std::thread producerThread(producer);

    // Launch consumer threads
    std::thread consumerThread1(consumer);
    std::thread consumerThread2(consumer);

    // Wait for all threads to complete
    producerThread.join();
    consumerThread1.join();
    consumerThread2.join();

    return 0;
}
```

### Notes:
- **Capacity**: Make sure the queue’s capacity is sufficiently large to handle your application's data throughput. 
- **Blocking Behavior**: The current implementation is non-blocking, meaning consumers will return `false` if there is 
no data to read.
