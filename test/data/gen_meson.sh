#!/bin/bash

set -x
set -e

case "$OSTYPE" in
  darwin*)
    CXX=$(xcrun --sdk macosx --find clang++)
    CXXFLAGS="-isysroot $(xcrun --sdk macosx --show-sdk-path) -mmacosx-version-min=10.11.0"
    ;;
  linux*)
    CXX=clang++
    ;;
  *) 1>&2 echo "Not supported OS: $OSTYPE"; exit 1;;
esac

rm -rf build
CXX="$CXX" CXXFLAGS="$CXXFLAGS" meson build
ninja -C build -d keepdepfile -v
