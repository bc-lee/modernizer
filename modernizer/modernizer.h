#ifndef MODERNIZER_MODERNIZER_H_
#define MODERNIZER_MODERNIZER_H_

#include <filesystem>
#include <thread>

#include "llvm/Support/raw_ostream.h"

namespace modernizer {

inline constexpr const char* kModernizeMacro = "RTC_DISALLOW_COPY_AND_ASSIGN";

struct RunModernizerOptions {
  std::filesystem::path project_root;
  std::filesystem::path compile_commands;
  int num_jobs = std::thread::hardware_concurrency();
  bool in_place = false;
  llvm::raw_ostream* out_stream = nullptr;
};

int RunModernizer(const RunModernizerOptions& options);

}  // namespace modernizer

#endif  // MODERNIZER_MODERNIZER_H_
