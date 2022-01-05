#include <thread>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetSelect.h"
#include "modernizer/modernizer.h"

ABSL_FLAG(std::string, project_root, "", "Path of project root");
ABSL_FLAG(std::string, compile_commands, "", "Path of compile_commands.json");
ABSL_FLAG(std::string, source_pattern, "", "Source file pattern");
ABSL_FLAG(bool, in_place, false, "Inplace edit <file>s, if specified.");
ABSL_FLAG(int,
          jobs,
          std::thread::hardware_concurrency(),
          "Run N jobs in parallel");

int main(int argc, char* argv[]) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();
  absl::SetProgramUsageMessage(
      "Usage: ./modernizer --project_root=/path/to/project "
      "--compile_commands=/path/to/project/out/compile_commands.json");
  absl::ParseCommandLine(argc, argv);

  modernizer::RunModernizerOptions modernizer_options{
      .project_root = absl::GetFlag(FLAGS_project_root),
      .compile_commands = absl::GetFlag(FLAGS_compile_commands),
      .source_file_pattern = absl::GetFlag(FLAGS_source_pattern),
      .num_jobs = absl::GetFlag(FLAGS_jobs),
      .in_place = absl::GetFlag(FLAGS_in_place),
      .out_stream =
          (absl::GetFlag(FLAGS_in_place) ? &llvm::nulls() : &llvm::outs())};
  int run_result = modernizer::RunModernizer(modernizer_options);
  return run_result;
}
