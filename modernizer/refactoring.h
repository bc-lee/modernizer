#ifndef MODERNIZER_REFACTORING_H_
#define MODERNIZER_REFACTORING_H_

#include <map>
#include <string>

#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/Core/Replacement.h"
#include "llvm/ADT/StringRef.h"

namespace modernizer {

bool FormatAndApplyAllReplacements(
    const std::map<std::string, clang::tooling::Replacements>& FileToReplaces,
    clang::Rewriter& Rewrite,
    llvm::StringRef Style = "file");

}  // namespace modernizer

#endif  // MODERNIZER_REFACTORING_H_
