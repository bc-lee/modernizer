#ifndef BYTE_BUFFER_H_
#define BYTE_BUFFER_H_

#include <cstddef>

#include "rtc_base/constructor_magic.h"

template <class BufferClassT>
class ByteBufferWriterT {
 public:
  ByteBufferWriterT(const char* bytes, size_t len);

  const char* Data() const { return buffer_.data(); }

 private:
  BufferClassT buffer_;
  RTC_DISALLOW_COPY_AND_ASSIGN(ByteBufferWriterT);
};

class ByteBufferReader {
 public:
  ByteBufferReader(const char* bytes, size_t len);

  // Initializes buffer from a zero-terminated string.
  explicit ByteBufferReader(const char* bytes);
  bool Consume(size_t size);

 protected:
  void Construct(const char* bytes, size_t size);

  const char* bytes_;
  size_t size_;
  size_t start_;
  size_t end_;

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(ByteBufferReader);
};

#endif  // BYTE_BUFFER_H_
