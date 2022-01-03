#ifndef MODERNIZER_FILESYSTEM_H_
#define MODERNIZER_FILESYSTEM_H_

#include <filesystem>

#include "llvm/Support/Error.h"

namespace modernizer {

llvm::Expected<std::filesystem::path> Canonical(
    const std::filesystem::path& path);

llvm::Expected<std::filesystem::path> Relative(
    const std::filesystem::path& path,
    const std::filesystem::path& base);

}  // namespace modernizer

#endif  // MODERNIZER_FILESYSTEM_H_
