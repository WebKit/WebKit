#!/usr/bin/env bash

set -e
set -o pipefail

rm -rf CMakeFiles &&
rm -f CMakeCache.txt &&
rm -f Makefile &&
rm -f cmake_install.cmake
