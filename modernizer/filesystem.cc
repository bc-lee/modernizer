#include "modernizer/filesystem.h"

namespace modernizer {

llvm::Expected<std::filesystem::path> Canonical(
    const std::filesystem::path& path) {
  std::error_code ec;
  auto result = std::filesystem::canonical(path, ec);
  if (ec) {
    return llvm::errorCodeToError(ec);
  }
  return result;
}

llvm::Expected<std::filesystem::path> Relative(
    const std::filesystem::path& path,
    const std::filesystem::path& base) {
  std::error_code ec;
  auto result = std::filesystem::relative(path, base, ec);
  if (ec) {
    return llvm::errorCodeToError(ec);
  }
  return result;
}

}  // namespace modernizer
