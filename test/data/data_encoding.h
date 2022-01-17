#ifndef DATA_ENCODING_H_
#define DATA_ENCODING_H_

#include <cstdint>
#include <optional>
#include <string>

#include "rtc_base/constructor_magic.h"

class BitstreamReader {};
class FixedLengthEncodingParameters {};

class FixedLengthDeltaDecoder final {
 public:
  static bool IsSuitableDecoderFor(const std::string& input);

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

  RTC_DISALLOW_COPY_AND_ASSIGN(FixedLengthDeltaDecoder);
};

#endif  // DATA_ENCODING_H_
