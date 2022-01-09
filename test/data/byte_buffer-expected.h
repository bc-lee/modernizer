#ifndef BYTE_BUFFER_H_
#define BYTE_BUFFER_H_

#include <cstddef>

#include "rtc_base/constructor_magic.h"

template <class BufferClassT>
class ByteBufferWriterT {
 public:
  ByteBufferWriterT(const char* bytes, size_t len);

  ByteBufferWriterT(const ByteBufferWriterT&) = delete;
  ByteBufferWriterT& operator=(const ByteBufferWriterT&) = delete;

  const char* Data() const { return buffer_.data(); }

 private:
  BufferClassT buffer_;
};

#endif  // BYTE_BUFFER_H_
