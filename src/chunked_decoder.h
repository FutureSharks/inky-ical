#ifndef CHUNKED_DECODER_H
#define CHUNKED_DECODER_H

/*
 * Decoder for HTTP/1.1 chunked transfer encoding (RFC 9112 section 7.1).
 *
 * HTTPClient only de-chunks inside writeToStream(), which buffers the whole
 * body; getStreamPtr() hands back the raw socket with the chunk framing still
 * in it. Parsing an .ics straight off the socket therefore needs the framing
 * stripped here, or chunk-size lines such as "7ff2" reach the parser as if
 * they were calendar properties.
 *
 * Deliberately free of any Arduino dependency so the host tests exercise this
 * exact code rather than a reimplementation of it. Callers supply a Source
 * that yields one byte at a time.
 *
 * The wire format is:
 *
 *     7ff2\r\n                  chunk size in hex, optional ";ext" suffix
 *     <32754 bytes of body>
 *     \r\n                      terminator, not part of the body
 *     ...
 *     0\r\n                     terminal chunk
 *     \r\n                      optional trailers, then a blank line
 */
class ChunkedDecoder
{
public:
  struct Source
  {
    virtual ~Source() {}
    // Next raw byte, or -1 once no more will arrive.
    virtual int readByte() = 0;
  };

  // `chunked` false makes this a pass-through, for responses that carry a
  // Content-Length instead. Keeping both cases behind one type means the
  // caller has a single code path regardless of how the body is framed.
  explicit ChunkedDecoder(Source &source, bool chunked = true)
      : source(source), chunked(chunked) {}

  // Next decoded body byte, or -1 at the end of the body.
  int read()
  {
    if (!chunked)
    {
      return source.readByte();
    }
    if (finished)
    {
      return -1;
    }
    if (remaining == 0 && !beginNextChunk())
    {
      return -1;
    }
    int ch = source.readByte();
    if (ch < 0)
    {
      // Ran out of data mid-chunk: the body was truncated.
      truncated = true;
      finished = true;
      return -1;
    }
    remaining--;
    return ch;
  }

  // True if the stream ended mid-chunk or the framing did not parse, meaning
  // the body was incomplete. Distinguishes a clean end from a broken one.
  bool failed() const { return truncated || malformed; }
  bool wasTruncated() const { return truncated; }
  bool wasMalformed() const { return malformed; }

private:
  Source &source;
  bool chunked;
  long remaining = 0;
  bool finished = false;
  bool truncated = false;
  bool malformed = false;
  bool at_first_chunk = true;

  static int hexValue(int ch)
  {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
  }

  void fail(bool is_truncation)
  {
    if (is_truncation)
    {
      truncated = true;
    }
    else
    {
      malformed = true;
    }
    finished = true;
  }

  // Consume bytes up to and including the next line ending.
  bool skipToEndOfLine()
  {
    while (true)
    {
      int ch = source.readByte();
      if (ch < 0) return false;
      if (ch == '\n') return true;
      if (ch == '\r')
      {
        int next = source.readByte();
        if (next < 0) return false;
        return next == '\n';
      }
    }
  }

  // Consume the CRLF that terminates a data chunk.
  bool consumeChunkTerminator()
  {
    int ch = source.readByte();
    if (ch == '\r')
    {
      ch = source.readByte();
    }
    return ch == '\n';
  }

  bool beginNextChunk()
  {
    if (!at_first_chunk && !consumeChunkTerminator())
    {
      fail(true);
      return false;
    }
    at_first_chunk = false;

    long size = 0;
    int digits = 0;
    while (true)
    {
      int ch = source.readByte();
      if (ch < 0)
      {
        fail(true);
        return false;
      }
      if (ch == '\n')
      {
        break;
      }
      if (ch == '\r')
      {
        int next = source.readByte();
        if (next != '\n')
        {
          fail(false);
          return false;
        }
        break;
      }
      if (ch == ';')
      {
        // Chunk extension; nothing here needs it.
        if (!skipToEndOfLine())
        {
          fail(true);
          return false;
        }
        break;
      }
      int value = hexValue(ch);
      if (value < 0)
      {
        fail(false);
        return false;
      }
      size = size * 16 + value;
      digits++;
    }

    if (digits == 0)
    {
      fail(false);
      return false;
    }
    if (size == 0)
    {
      // Terminal chunk. Any trailers that follow are of no interest.
      finished = true;
      return false;
    }

    remaining = size;
    return true;
  }
};

#endif
