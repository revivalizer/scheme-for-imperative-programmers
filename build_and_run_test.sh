#!/usr/bin/env bash
set -euo pipefail

CXX=clang++
CXXFLAGS="
-std=c++11
-Wall
-Wextra
-Wpedantic
-Werror
-Wshadow
-Wconversion
-Wsign-conversion
-Wnull-dereference
-Wdouble-promotion
-Wformat=2
-fno-common
"

LDFLAGS=""

$CXX $CXXFLAGS test.cpp -o pragmatic-scheme-tests && ./pragmatic-scheme-tests