#ifndef HTTP_BYTE_SOURCE_H
#define HTTP_BYTE_SOURCE_H

#include <Arduino.h>
#include <WiFiClient.h>
#include "chunked_decoder.h"

/*
 * Blocking byte source over an HTTP socket, for feeding ChunkedDecoder.
 *
 * Reads in blocks rather than a byte at a time: every single-byte read on a
 * TLS connection is a call into mbedTLS, and a feed of a few hundred KB makes
 * that hundreds of thousands of them.
 *
 * Deciding when the body has actually ended is the delicate part. A
 * WiFiClient returns nothing whenever its buffer happens to be empty, which
 * says nothing about whether more is coming, and on a TLS socket available()
 * can report zero while an undecrypted record is still pending. Treating
 * either as the end truncates the body, so the end is only accepted once the
 * connection is closed and a further read attempt also comes back empty.
 */
class HttpByteSource : public ChunkedDecoder::Source
{
public:
  HttpByteSource(WiFiClient &client, unsigned long timeout_ms)
      : client(client), timeout_ms(timeout_ms) {}

  int readByte() override
  {
    if (buffer_pos >= buffer_len && !refill())
    {
      return -1;
    }
    return buffer[buffer_pos++];
  }

  // Bytes taken off the socket, including any transfer framing.
  size_t bytesRead() const { return total_bytes; }
  // True if the read gave up waiting for more body rather than seeing the end.
  bool timedOut() const { return timed_out; }

private:
  static const size_t BUFFER_SIZE = 512;

  bool refill()
  {
    buffer_pos = 0;
    buffer_len = 0;

    unsigned long start = millis();
    while (true)
    {
      int ready = client.available();
      if (ready > 0)
      {
        size_t want = (size_t)ready < BUFFER_SIZE ? (size_t)ready : BUFFER_SIZE;
        int got = client.read(buffer, want);
        if (got > 0)
        {
          buffer_len = (size_t)got;
          total_bytes += (size_t)got;
          return true;
        }
      }

      if (!client.connected())
      {
        // Closed, but drain anything mbedTLS still has buffered before
        // accepting that as the end of the body.
        int got = client.read(buffer, BUFFER_SIZE);
        if (got > 0)
        {
          buffer_len = (size_t)got;
          total_bytes += (size_t)got;
          return true;
        }
        return false;
      }

      if (millis() - start > timeout_ms)
      {
        timed_out = true;
        return false;
      }
      delay(1);
    }
  }

  WiFiClient &client;
  unsigned long timeout_ms;
  uint8_t buffer[BUFFER_SIZE];
  size_t buffer_pos = 0;
  size_t buffer_len = 0;
  size_t total_bytes = 0;
  bool timed_out = false;
};

#endif
