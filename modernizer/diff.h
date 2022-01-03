#ifndef MODERNIZER_DIFF_H_
#define MODERNIZER_DIFF_H_

#include <string_view>

#include "llvm/Support/raw_ostream.h"

namespace modernizer {

bool CreateDiff(std::string_view file_name,
                std::string_view left,
                std::string_view right,
                llvm::raw_ostream& stream);

}  // namespace modernizer

#endif  // MODERNIZER_DIFF_H_
