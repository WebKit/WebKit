#!/bin/sh

# Capstone disassembler engine (www.capstone-engine.org)
# Build Capstone libs for specified architecture, or all if none is specified (libcapstone.so & libcapstone.a) on *nix with CMake & make
# By Nguyen Anh Quynh, Jorn Vernee, 2019

CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=Release"
# Uncomment below line to compile in Diet mode
# CMAKE_FLAGS+=" -DCAPSTONE_BUILD_DIET=ON"

case $1 in
  ARM)
    ARCH=ARM
    ;;
  ARM64)
    ARCH=ARM64
    ;;
  M68K)
    ARCH=M68K
    ;;
  MIPS)
    ARCH=MIPS
    ;;
  PowerPC)
    ARCH=PPC
    ;;
  Sparc)
    ARCH=SPARC
    ;;
  SystemZ)
    ARCH=SYSZ
    ;;
  XCore)
    ARCH=XCORE
    ;;
  x86)
    ARCH=X86
    ;;
  TMS320C64x)
    ARCH=TMS320C64X
    ;;
  M680x)
    ARCH=M680X
    ;;
  EVM)
    ARCH=EVM
    ;;
  MOS65XX)
    ARCH=MOS65XX
    ;;
  WASM)
    ARCH=WASM
    ;;
  BPF)
    ARCH=BPF
    ;;
  RISCV)
    ARCH=RISCV
    ;;
  *)
    ;;
esac

if [ -z "${ARCH}" ]; then
  CMAKE_FLAGS="${CMAKE_FLAGS} -DCAPSTONE_ARCHITECTURE_DEFAULT=ON"
else
  CMAKE_FLAGS="${CMAKE_FLAGS} -DCAPSTONE_ARCHITECTURE_DEFAULT=OFF -DCAPSTONE_${ARCH}_SUPPORT=ON"
fi

cmake ${CMAKE_FLAGS} ..

make -j8
