#!/bin/bash

set -x
set -e

cd "$(dirname "$0")"

case "$OSTYPE" in
  darwin*)
    if [[ -z ${CXX} ]]; then
      CXX=$(xcrun --sdk macosx --find clang++)
    fi
    CXXFLAGS="-isysroot $(xcrun --sdk macosx --show-sdk-path) -mmacosx-version-min=10.11.0"
    ;;
  linux*)
    if [[ -z ${CXX} ]]; then
      CXX=clang++
    fi
    ;;
  *) 1>&2 echo "Not supported OS: $OSTYPE"; exit 1;;
esac

rm -rf build
CXX="$CXX" CXXFLAGS="$CXXFLAGS" meson build
ninja -C build -d keepdepfile -v
