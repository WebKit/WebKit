#!/bin/bash
set -ev

# Download & build RISC-V Clang toolchain & QEMU emulator. 
# RISC-V Clang is for cross compile with  the RISC-V Vector ISA.
# RISC-V QEMU is used to run the test suite.
#
# Requirements: Linux host w/ working C++ compiler, git, cmake, ninja, wget, tar

# NOTE: this script must be run from the top-level directory of the LIBYUV_SRC_DIR.

RISCV_TRIPLE="riscv64-unknown-linux-gnu"
RISCV_QEMU="qemu-riscv64"

LIBYUV_SRC_DIR=$(pwd)
BUILD_DIR="$LIBYUV_SRC_DIR"/build-toolchain-qemu
INSTALL_QEMU="$BUILD_DIR"/riscv-qemu
INSTALL_CLANG="$BUILD_DIR"/riscv-clang

LLVM_VERSION="16.0.0"
LLVM_NAME=llvm-project-"$LLVM_VERSION".src

RISCV_GNU_TOOLCHAIN="$BUILD_DIR"/riscv-gnu-toolchain
RISCV_CLANG_TOOLCHAIN="$BUILD_DIR"/"$LLVM_NAME"

QEMU_NAME="qemu-7.0.0"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Download and install RISC-V GNU Toolchain (needed to build Clang)
if [ ! -d "$RISCV_GNU_TOOLCHAIN" ]
then
  git clone git@github.com:riscv/riscv-gnu-toolchain.git
  pushd "$RISCV_GNU_TOOLCHAIN"
  git submodule update --init --recursive
  ./configure --with-cmodel=medany --prefix="$INSTALL_CLANG"
  ionice nice make linux -j `nproc` install
  popd
fi

# Download Clang toolchain & build cross compiler
if [ ! -d "$RISCV_CLANG_TOOLCHAIN" ]
then
  wget https://github.com/llvm/llvm-project/releases/download/llvmorg-"$LLVM_VERSION"/"$LLVM_NAME".tar.xz
  tar xvJf "$LLVM_NAME".tar.xz
  pushd "$RISCV_CLANG_TOOLCHAIN"
	cmake -DCMAKE_INSTALL_PREFIX="$INSTALL_CLANG" \
      -DCMAKE_BUILD_TYPE=Release \
      -DLLVM_TARGETS_TO_BUILD="RISCV" \
      -DLLVM_ENABLE_PROJECTS="clang" \
      -DLLVM_DEFAULT_TARGET_TRIPLE="$RISCV_TRIPLE" \
      -DLLVM_INSTALL_TOOLCHAIN_ONLY=On \
      -DDEFAULT_SYSROOT=../sysroot \
      -G "Ninja" "$RISCV_CLANG_TOOLCHAIN"/llvm
	ionice nice ninja -j `nproc`
	ionice nice ninja -j `nproc` install
  popd
  pushd "$INSTALL_CLANG"/bin
  ln -sf clang "$RISCV_TRIPLE"-clang
  ln -sf clang++ "$RISCV_TRIPLE"-clang++
  popd
fi

# Download QEMU and build the riscv64 Linux usermode emulator
if [ ! -d "$QEMU_NAME" ]
then
  wget https://download.qemu.org/"$QEMU_NAME".tar.xz
  tar xvJf "$QEMU_NAME".tar.xz
  pushd "$QEMU_NAME"
  ./configure --target-list=riscv64-linux-user --prefix="$INSTALL_QEMU"
  ionice nice make -j `nproc` install
  popd
fi
