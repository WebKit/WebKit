#!/bin/sh

# Capstone Disassembly Engine
# By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2019

# Note: to cross-compile "nix32" on Linux, package gcc-multilib is required.

MAKE_JOBS=${MAKE_JOBS:-4}

# build Android lib for only one supported architecture
build_android() {
  if [ -z "$NDK" ]; then
    echo "ERROR! Please set \$NDK to point at your Android NDK directory."
    exit 1
  fi

  HOSTOS=$(uname -s | tr 'LD' 'ld')
  HOSTARCH=$(uname -m)

  TARGARCH="$1"
  shift

  case "$TARGARCH" in
    arm)
      [ -n "$APILEVEL" ] || APILEVEL="android-14"  # default to ICS
      CROSS=arm-linux-androideabi
      ;;
    arm64)
      [ -n "$APILEVEL" ] || APILEVEL="android-21"  # first with arm64
      CROSS=aarch64-linux-android
      ;;

    *)
      echo "ERROR! Building for Android on $1 is not currently supported."
      exit 1
      ;;
  esac

  STANDALONE=`realpath android-ndk-${TARGARCH}-${APILEVEL}`

  [ -d $STANDALONE ] || {
      python ${NDK}/build/tools/make_standalone_toolchain.py \
             --arch ${TARGARCH} \
             --api ${APILEVEL##*-} \
             --install-dir ${STANDALONE}
  }

  ANDROID=1 CROSS="${STANDALONE}/${CROSS}/bin" CFLAGS="--sysroot=${STANDALONE}/sysroot" ${MAKE} $*
}

# build iOS lib for all iDevices, or only specific device
build_iOS() {
  IOS_SDK=`xcrun --sdk iphoneos --show-sdk-path`
  IOS_CC=`xcrun --sdk iphoneos -f clang`
  IOS_CFLAGS="-Os -Wimplicit -isysroot $IOS_SDK"
  IOS_LDFLAGS="-isysroot $IOS_SDK"
  if [ -z "$1" ]; then
    # build for all iDevices
    IOS_ARCHS="armv7 armv7s arm64"
  else
    IOS_ARCHS="$1"
  fi
  export CC="$IOS_CC"
  export LIBARCHS="$IOS_ARCHS"
  CFLAGS="$IOS_CFLAGS" LDFLAGS="$IOS_LDFLAGS" MACOS_UNIVERSAL=yes ${MAKE}
}

install() {
  # Mac OSX needs to find the right directory for pkgconfig
  if [ "$UNAME" = Darwin ]; then
    # we are going to install into /usr/local, so remove old installs under /usr
    rm -rf /usr/lib/libcapstone.*
    rm -rf /usr/include/capstone
    if [ "${HOMEBREW_CAPSTONE}" != 1 ]; then
      # find the directory automatically, so we can support both Macport & Brew
      export PKGCFGDIR="$(pkg-config --variable pc_path pkg-config | cut -d ':' -f 1)"
    fi
    ${MAKE} install
  else  # not OSX
    test -d /usr/lib64 && ${MAKE} LIBDIRARCH=lib64
    ${MAKE} install
  fi
}

uninstall() {
  # Mac OSX needs to find the right directory for pkgconfig
  if [ "$UNAME" = "Darwin" ]; then
    # find the directory automatically, so we can support both Macport & Brew
    export PKGCFGDIR="$(pkg-config --variable pc_path pkg-config | cut -d ':' -f 1)"
    ${MAKE} uninstall
  else  # not OSX
    test -d /usr/lib64 && LIBDIRARCH=lib64
    ${MAKE} uninstall
  fi
}

UNAME="${UNAME:-$(uname)}"

if [ "$UNAME" = SunOS ]; then
  MAKE="${MAKE:-gmake}"
  export INSTALL_BIN=ginstall
  export CC=gcc
fi

MAKE="${MAKE:-make}"

if echo "$UNAME" | grep -q BSD; then
  MAKE=gmake
  export PREFIX=/usr/local
fi

MAKE="$MAKE -j${MAKE_JOBS}"

TARGET="$1"
[ $# -gt 0 ] && shift

case "$TARGET" in
  "" | "default" ) ${MAKE} "$@";;
  "debug" ) \
      CAPSTONE_USE_SYS_DYN_MEM=yes \
      CAPSTONE_STATIC=yes \
      CFLAGS='-DCAPSTONE_DEBUG -O0 -g -fsanitize=address' \
      LDFLAGS='-fsanitize=address' \
      ${MAKE} "$@";;
  "install" ) install;;
  "uninstall" ) uninstall;;
  "nix32" ) CFLAGS=-m32 LDFLAGS=-m32 ${MAKE} "$@";;
  "cross-win32" ) CROSS=i686-w64-mingw32- ${MAKE} "$@";;
  "cross-win64" ) CROSS=x86_64-w64-mingw32- ${MAKE} "$@";;
  "cygwin-mingw32" ) CROSS=i686-pc-mingw32- ${MAKE} "$@";;
  "cygwin-mingw64" ) CROSS=x86_64-w64-mingw32- ${MAKE} "$@";;
  "cross-android" ) build_android "$@";;
  "cross-android64" ) CROSS=aarch64-linux-gnu- ${MAKE} "$@";;	# Linux cross build
  "clang" ) CC=clang ${MAKE} "$@";;
  "gcc" ) CC=gcc ${MAKE} "$@";;
  "ios" ) build_iOS "$@";;
  "ios_armv7" ) build_iOS armv7 "$@";;
  "ios_armv7s" ) build_iOS armv7s "$@";;
  "ios_arm64" ) build_iOS arm64 "$@";;
  "osx-kernel" ) CAPSTONE_USE_SYS_DYN_MEM=yes \
      CAPSTONE_HAS_OSXKERNEL=yes \
      CAPSTONE_ARCHS=x86 \
      CAPSTONE_SHARED=no \
      CAPSTONE_BUILD_CORE_ONLY=yes \
      ${MAKE} "$@";;
  "mac-universal" ) MACOS_UNIVERSAL=yes ${MAKE} "$@";;
  "mac-universal-no" ) MACOS_UNIVERSAL=no ${MAKE} "$@";;
  "xlc31" ) CC=xlc CFLAGS=-q31 LDFLAGS=-q31 ${MAKE} "$@";;
  "xlc32" ) CC=xlc CFLAGS=-q32 LDFLAGS=-q32 ${MAKE} "$@";;
  "xlc64" ) CC=xlc CFLAGS=-q64 LDFLAGS=-q64 ${MAKE} "$@";;
  * )
    echo "Usage: $0 ["$(grep '^  "' $0 | cut -d '"' -f 2 | tr "\\n" "|")"]"
    exit 1;;
esac
