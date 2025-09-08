#include "ring_buffer.hpp"

#include <algorithm>
#include <cstring>

RingBuffer::RingBuffer(size_t capacity) : capacity_(capacity), head_(0), tail_(0) {
    buffer_ = new char[capacity_];
}

RingBuffer::~RingBuffer() { delete[] buffer_; }

bool RingBuffer::write(const char* data, size_t len) {
    if (len > available()) {
        size_t new_capacity = std::max(capacity_ * 2, capacity_ + len);
        resize(new_capacity);
    }

    size_t space_to_end = capacity_ - tail_;
    if (len <= space_to_end) {
        std::memcpy(buffer_ + tail_, data, len);
        tail_ += len;
    } else {
        std::memcpy(buffer_ + tail_, data, space_to_end);
        std::memcpy(buffer_, data + space_to_end, len - space_to_end);
        tail_ = len - space_to_end;
    }
    return true;
}

std::string_view RingBuffer::peek(size_t offset, size_t len) const {
    if (offset + len > size()) {
        return {};
    }
    size_t pos = (head_ + offset) % capacity_;
    if (pos + len <= capacity_) {
        return std::string_view(buffer_ + pos, len);
    }
    // Handle wrap-around
    char* temp = new char[len];
    size_t first_part = capacity_ - pos;
    std::memcpy(temp, buffer_ + pos, first_part);
    std::memcpy(temp + first_part, buffer_, len - first_part);
    std::string_view result(temp, len);
    delete[] temp;
    return result;
}

void RingBuffer::consume(size_t len) {
    if (len > size()) {
        len = size();
    }
    head_ = (head_ + len) % capacity_;
    if (head_ == tail_) {
        head_ = tail_ = 0;  // Reset to avoid fragmentation
    }
}

size_t RingBuffer::size() const {
    if (tail_ >= head_) {
        return tail_ - head_;
    }
    return capacity_ - head_ + tail_;
}

size_t RingBuffer::available() const { return capacity_ - size(); }

void RingBuffer::resize(size_t new_capacity) {
    char* new_buffer = new char[new_capacity];
    size_t data_size = size();
    if (data_size > 0) {
        if (head_ <= tail_) {
            std::memcpy(new_buffer, buffer_ + head_, data_size);
        } else {
            size_t first_part = capacity_ - head_;
            std::memcpy(new_buffer, buffer_ + head_, first_part);
            std::memcpy(new_buffer + first_part, buffer_, tail_);
        }
    }
    delete[] buffer_;
    buffer_ = new_buffer;
    capacity_ = new_capacity;
    head_ = 0;
    tail_ = data_size;
}