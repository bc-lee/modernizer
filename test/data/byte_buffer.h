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

#endif  // BYTE_BUFFER_H_
