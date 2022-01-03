#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

set -x
set -e

case "$OSTYPE" in
  darwin*)
    CXX="$SCRIPT_DIR/../../third_party/llvm-build/Release+Asserts/bin/clang++"
    CXXFLAGS="-nostdinc++ -isysroot $(xcrun --sdk macosx --show-sdk-path) \
     -mmacosx-version-min=10.11.0 \
     -isystem$SCRIPT_DIR/../../third_party/llvm-build/Release+Asserts/include/c++/v1 \
     -isystem$SCRIPT_DIR/../../third_party/llvm-build/Release+Asserts/lib/clang/14.0.0/include"
    ;;
  linux*)
    CXX=clang++
    ;;
  *) 1>&2 echo "Not supported OS: $OSTYPE"; exit 1;;
esac

rm -rf build
CXX="$CXX" CXXFLAGS="$CXXFLAGS" meson build
ninja -C build -d keepdepfile -v
