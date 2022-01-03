//===--- Refactoring.cpp - Framework for clang refactoring tools ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  Implements tools to support refactorings.
//
//===----------------------------------------------------------------------===//
//
//  Original Source:
//    https://github.com/llvm/llvm-project/blob/8d323d150610bed1feeb79d7a29c9958a4c8bcac/clang/lib/Tooling/Refactoring.cpp
//
//  Local Modification:
//    - Extract formatAndApplyAllReplacements function.
//
//    - Apply following patch:
//
// --- a/clang/lib/Tooling/Refactoring.cpp
// +++ b/clang/lib/Tooling/Refactoring.cpp
// @@ -85,7 +85,8 @@ bool formatAndApplyAllReplacements(
//      FileID ID = SM.getOrCreateFileID(Entry, SrcMgr::C_User);
//      StringRef Code = SM.getBufferData(ID);
//
// -    auto CurStyle = format::getStyle(Style, FilePath, "LLVM");
// +    auto CurStyle = format::getStyle(Style, FilePath, "LLVM", "",
// +                                     &Files.getVirtualFileSystem());
//      if (!CurStyle) {
//        llvm::errs() << llvm::toString(CurStyle.takeError()) << "\n";
//        return false;
//
//===----------------------------------------------------------------------===//

#include "modernizer/refactoring.h"

#include "clang/Basic/FileEntry.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Format/Format.h"

using namespace llvm;
using namespace clang;
using namespace clang::tooling;

namespace modernizer {

bool FormatAndApplyAllReplacements(
    const std::map<std::string, Replacements>& FileToReplaces,
    Rewriter& Rewrite,
    StringRef Style) {
  SourceManager& SM = Rewrite.getSourceMgr();
  FileManager& Files = SM.getFileManager();

  bool Result = true;
  for (const auto& FileAndReplaces : groupReplacementsByFile(
           Rewrite.getSourceMgr().getFileManager(), FileToReplaces)) {
    const std::string& FilePath = FileAndReplaces.first;
    auto& CurReplaces = FileAndReplaces.second;

    const FileEntry* Entry = nullptr;
    if (auto File = Files.getFile(FilePath))
      Entry = *File;

    FileID ID = SM.getOrCreateFileID(Entry, SrcMgr::C_User);
    StringRef Code = SM.getBufferData(ID);

    auto CurStyle = format::getStyle(Style, FilePath, "LLVM", "",
                                     &Files.getVirtualFileSystem());
    if (!CurStyle) {
      llvm::errs() << llvm::toString(CurStyle.takeError()) << "\n";
      return false;
    }

    auto NewReplacements =
        format::formatReplacements(Code, CurReplaces, *CurStyle);
    if (!NewReplacements) {
      llvm::errs() << llvm::toString(NewReplacements.takeError()) << "\n";
      return false;
    }
    Result = applyAllReplacements(*NewReplacements, Rewrite) && Result;
  }
  return Result;
}

}  // namespace modernizer
