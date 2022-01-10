#ifndef BYTE_BUFFER_H_
#define BYTE_BUFFER_H_

#include <cstddef>

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

class ByteBufferReader {
 public:
  ByteBufferReader(const char* bytes, size_t len);

  // Initializes buffer from a zero-terminated string.
  explicit ByteBufferReader(const char* bytes);

  ByteBufferReader(const ByteBufferReader&) = delete;
  ByteBufferReader& operator=(const ByteBufferReader&) = delete;

  bool Consume(size_t size);

 protected:
  void Construct(const char* bytes, size_t size);

  const char* bytes_;
  size_t size_;
  size_t start_;
  size_t end_;
};

#endif  // BYTE_BUFFER_H_
