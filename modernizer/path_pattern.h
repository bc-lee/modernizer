#ifndef MODERNIZER_PATH_PATTERN_H_
#define MODERNIZER_PATH_PATTERN_H_

#include <optional>
#include <vector>

#include "re2/re2.h"

namespace modernizer {

class PathPattern {
 public:
  ~PathPattern() = default;

  PathPattern(const PathPattern& other) = delete;
  PathPattern& operator=(const PathPattern& other) = delete;

  PathPattern(PathPattern&& other) = default;
  PathPattern& operator=(PathPattern&& other) = default;

  static std::optional<PathPattern> Create(std::string_view path_patterns);

  bool Match(std::string_view path) const;

 private:
  struct PatternElement {
    std::unique_ptr<re2::RE2> regex;
    bool negate = false;
  };

  explicit PathPattern(std::vector<PatternElement>&& patterns);

  std::vector<PatternElement> patterns_;
};

}  // namespace modernizer

#endif  // MODERNIZER_PATH_PATTERN_H_
