#ifndef PSRAM_BUFFER_H
#define PSRAM_BUFFER_H

#include <esp_heap_caps.h>
#include <stddef.h>

/*
 * Growable byte buffer held in PSRAM.
 *
 * The board carries 8MB of OPI PSRAM, so a few hundred KB of calendar costs
 * nothing there, whereas the internal heap has only ~280KB total and is
 * already carrying the WiFi and TLS buffers. Allocating explicitly rather
 * than through String keeps the body out of internal RAM by construction
 * instead of relying on the allocator's size heuristic.
 */
class PsramBuffer
{
public:
  PsramBuffer() {}
  ~PsramBuffer()
  {
    if (data_)
    {
      heap_caps_free(data_);
    }
  }

  // Non-copyable: two owners would double-free the allocation.
  PsramBuffer(const PsramBuffer &) = delete;
  PsramBuffer &operator=(const PsramBuffer &) = delete;

  // Returns false only if the buffer needed to grow and no memory was left.
  bool append(char ch)
  {
    if (length_ == capacity_ && !grow())
    {
      return false;
    }
    data_[length_++] = ch;
    return true;
  }

  const char *data() const { return data_; }
  size_t size() const { return length_; }

private:
  static const size_t INITIAL_CAPACITY = 64 * 1024;

  bool grow()
  {
    size_t wanted = capacity_ ? capacity_ * 2 : INITIAL_CAPACITY;
    char *moved = (char *)heap_caps_realloc(data_, wanted, MALLOC_CAP_SPIRAM);
    if (!moved)
    {
      // No PSRAM available (or not enabled): internal RAM is better than
      // failing outright, even though it may not fit.
      moved = (char *)heap_caps_realloc(data_, wanted, MALLOC_CAP_8BIT);
    }
    if (!moved)
    {
      return false;
    }
    data_ = moved;
    capacity_ = wanted;
    return true;
  }

  char *data_ = nullptr;
  size_t length_ = 0;
  size_t capacity_ = 0;
};

#endif
