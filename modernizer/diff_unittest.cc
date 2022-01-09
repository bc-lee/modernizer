#include "modernizer/diff.h"

#include "gtest/gtest.h"

TEST(DiffTest, Simple) {
  std::string_view old_str = "line2\nline3\nline4\n";
  std::string_view new_str = "line2\nline3\nline4\nline5\n";

  std::string result;
  llvm::raw_string_ostream result_stream(result);

  modernizer::CreateDiff("file.txt", old_str, new_str, result_stream);
  result_stream.flush();

  std::string_view expected =
      "--- a/file.txt\n"
      "+++ b/file.txt\n"
      "@@ -1,3 +1,4 @@\n"
      " line2\n"
      " line3\n"
      " line4\n"
      "+line5\n";

  ASSERT_EQ(result, expected);
}
