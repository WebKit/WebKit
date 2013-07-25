#!/bin/sh

set -e
set -x

if test "x${LLVM_PATH}" == "x"
then
    path=
else
    path="${LLVM_PATH}/bin/"
fi

./combineModules.rb > temp.ll
${path}llvm-as temp.ll
./ReducedFTL temp.bc "$@"
