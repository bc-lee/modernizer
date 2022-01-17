#ifndef DATA_ENCODING_H_
#define DATA_ENCODING_H_

#include <cstdint>
#include <optional>
#include <string>

class BitstreamReader {};
class FixedLengthEncodingParameters {};

class FixedLengthDeltaDecoder final {
 public:
  static bool IsSuitableDecoderFor(const std::string& input);

  FixedLengthDeltaDecoder(const FixedLengthDeltaDecoder&) = delete;
  FixedLengthDeltaDecoder& operator=(const FixedLengthDeltaDecoder&) = delete;

 private:
  static std::unique_ptr<FixedLengthDeltaDecoder> Create(
      const std::string& input,
      std::optional<uint64_t> base,
      size_t num_of_deltas);

  FixedLengthDeltaDecoder(BitstreamReader reader,
                          const FixedLengthEncodingParameters& params,
                          std::optional<uint64_t> base,
                          size_t num_of_deltas);

  const std::optional<uint64_t> base_;

  // The number of values to be known to be decoded.
  const size_t num_of_deltas_;
};

#endif  // DATA_ENCODING_H_
