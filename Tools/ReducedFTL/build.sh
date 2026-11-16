#!/bin/sh

set -e
set -x

if test "x${LLVM_PATH}" == "x"
then
    configPath="llvm-config"
else
    configPath="${LLVM_PATH}/bin/llvm-config"
fi

clang -c -o ReducedFTL.o ReducedFTL.c `${configPath} --cppflags --cflags` -g
clang++ -o ReducedFTL ReducedFTL.o -stdlib=libc++ `${configPath} --ldflags --libs` -lcurses
