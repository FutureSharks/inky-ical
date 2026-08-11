#ifndef BUFFER_READER_H
#define BUFFER_READER_H

#include "uICAL.h"

/*
 * uICAL reader over a plain block of memory.
 *
 * The response body has to be drained from the socket faster than the parser
 * can consume it - parsing straight off the connection lets the TCP receive
 * window fill, at which point the server stops sending and the transfer
 * stalls mid-body. So the body is buffered first and parsed afterwards, and
 * this is what the parser reads from.
 *
 * Deliberately takes a bare pointer rather than a String: the buffer lives in
 * PSRAM and copying it into a String would double the peak memory for no
 * reason. Free of Arduino types other than uICAL's own string, so the host
 * tests exercise this exact code.
 */
// uICAL::string wraps Arduino String on the device and std::string on the
// host, and the two spell "assign these N bytes" differently.
static inline void assign_bytes(uICAL::string &st, const char *data, size_t count)
{
#ifdef ARDUINO
  st = "";
  st.concat(data, (unsigned int)count);
#else
  st.assign(data, count);
#endif
}

class BufferReader : public uICAL::istream
{
public:
  BufferReader(const char *data, size_t length) : data(data), length(length), pos(0) {}

  char peek() const override { return pos < length ? data[pos] : (char)-1; }
  char get() override { return pos < length ? data[pos++] : (char)-1; }

  bool readuntil(uICAL::string &st, char delim, size_t maxLen = 0) override
  {
    if (pos >= length)
    {
      return false;
    }

    size_t start = pos;
    while (pos < length && data[pos] != delim)
    {
      pos++;
    }
    size_t count = pos - start;
    if (pos < length)
    {
      pos++;  // Step over the delimiter itself.
    }
    if (maxLen > 0 && count > maxLen)
    {
      count = maxLen;
    }

    assign_bytes(st, data + start, count);
    return true;
  }

private:
  const char *data;
  size_t length;
  size_t pos;
};

#endif
