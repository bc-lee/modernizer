#include "modernizer/path_pattern.h"

#include "absl/strings/str_split.h"
#include "llvm/Support/raw_ostream.h"

namespace modernizer {

PathPattern::PathPattern(std::vector<PatternElement>&& patterns)
    : patterns_(std::move(patterns)) {}

std::optional<PathPattern> PathPattern::Create(std::string_view path_patterns) {
  std::vector<PatternElement> compiled_patterns;
  auto split = absl::StrSplit(path_patterns, ":", absl::SkipEmpty());
  for (auto iter = split.begin(); !iter.at_end(); ++iter) {
    const char* str_begin = iter->begin();
    const char* str_end = iter->end();
    bool negate = (*str_begin == '!');
    if (negate) {
      ++str_begin;
    }
    if (str_begin == str_end) {
      llvm::errs() << "Empty path pattern\n";
      return std::nullopt;
    }
    bool absolute_path = (*str_begin == '/');
    if (absolute_path) {
      ++str_begin;
    }
    std::string_view remainder;
    if (str_end - str_begin > 0) {
      remainder = std::string_view(str_begin, str_end - str_begin);
    }

    std::string pattern_string;
    llvm::raw_string_ostream pattern_string_stream(pattern_string);
    pattern_string_stream << "^";
    if (absolute_path) {
      pattern_string_stream << "/";
    } else {
      pattern_string_stream << ".*";
    }

    if (!remainder.empty()) {
      pattern_string_stream << remainder << ".*";
    } else if (absolute_path) {
      pattern_string_stream << ".*";
    }
    pattern_string_stream << "$";
    pattern_string_stream.flush();

    std::unique_ptr<re2::RE2> pattern =
        std::make_unique<re2::RE2>(pattern_string);
    if (!pattern->ok()) {
      llvm::errs() << "Bad regex pattern: " << pattern_string << "\n";
      return std::nullopt;
    }
    compiled_patterns.push_back(
        PatternElement{.regex = std::move(pattern), .negate = negate});
  }
  if (compiled_patterns.empty()) {
    llvm::errs() << "No patterns given\n";
    return std::nullopt;
  }
  return PathPattern(std::move(compiled_patterns));
}

bool PathPattern::Match(std::string_view path) const {
  bool result = false;
  std::string converted_path;
  llvm::raw_string_ostream converted_path_stream(converted_path);
  converted_path_stream << "/";
  converted_path_stream << path;
  converted_path_stream.flush();

  for (const auto& pattern : patterns_) {
    if (re2::RE2::FullMatch(converted_path, *pattern.regex)) {
      result = !pattern.negate;
    }
  }
  return result;
}

}  // namespace modernizer
