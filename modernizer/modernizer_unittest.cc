#include "modernizer/modernizer.h"

#include <filesystem>

#include "gtest/gtest.h"
#include "llvm/Support/raw_ostream.h"

namespace modernizer {
TEST(ModernizerTest, Test) {
  std::filesystem::path test_data_root(TESTDATA_ROOT);
  ASSERT_TRUE(std::filesystem::is_directory(test_data_root));
  auto test_compile_commands =
      test_data_root / "build" / "compile_commands.json";

  std::string output;
  llvm::raw_string_ostream output_stream(output);

  RunModernizerOptions options{.project_root = test_data_root,
                               .compile_commands = test_compile_commands,
                               .in_place = false,
                               .out_stream = &output_stream};
  int result = modernizer::RunModernizer(options);
  ASSERT_EQ(result, 0);
  output_stream.flush();

  std::string expected =
      "--- a/input.cc\n"
      "+++ b/input.cc\n"
      "@@ -8,11 +8,13 @@\n"
      "  public:\n"
      "   Foo();\n"
      "   ~Foo();\n"
      "+\n"
      "+  Foo(const Foo&) = delete;\n"
      "+  Foo& operator=(const Foo&) = delete;\n"
      " \n"
      "   std::string foo();\n"
      " \n"
      "  private:\n"
      "-  RTC_DISALLOW_COPY_AND_ASSIGN(Foo);\n"
      "   std::string foo_{\"foo\"};\n"
      " };\n"
      " \n";
  ASSERT_EQ(expected, output);
}

}  // namespace modernizer
