#include "modernizer/diff.h"

#include "absl/strings/str_split.h"
#include "gtest/internal/gtest-internal.h"

namespace modernizer {

namespace {

std::vector<std::string> CreateLinedBuffer(std::string_view view) {
  // TODO(bc-lee): Handle case with trailing newline character.
  std::vector<std::string> lines = absl::StrSplit(view, "\n");
  if (!lines.empty() && lines.back().empty()) {
    lines.pop_back();
  }
  return lines;
}

}  // namespace

bool CreateDiff(std::string_view file_name,
                std::string_view before,
                std::string_view after,
                llvm::raw_ostream& stream) {
  std::vector<std::string> before_lines = CreateLinedBuffer(before);
  std::vector<std::string> after_lines = CreateLinedBuffer(after);

  std::string result = ::testing::internal::edit_distance::CreateUnifiedDiff(
      before_lines, after_lines, 3);
  stream << "--- a/" << file_name << "\n+++ b/" << file_name << "\n";
  stream << result;
  return true;
}

}  // namespace modernizer
