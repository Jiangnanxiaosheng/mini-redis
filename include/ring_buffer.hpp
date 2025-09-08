#pragma once

#include <cstddef>
#include <string_view>

class RingBuffer {
public:
    RingBuffer(size_t capacity = 1024);
    ~RingBuffer();

    // Write data to the buffer
    bool write(const char* data, size_t len);

    // Read data into a string_view without removing it
    std::string_view peek(size_t offset, size_t len) const;

    // Consume (remove) data from the buffer
    void consume(size_t len);

    // Get available data size
    size_t size() const;

    // Get available space for writing
    size_t available() const;

private:
    char* buffer_;
    size_t capacity_;
    size_t head_;  // Points to the start of data
    size_t tail_;  // Points to the end of data

    // Resize buffer if needed
    void resize(size_t new_capacity);
};