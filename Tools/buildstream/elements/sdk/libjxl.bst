kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst
- sdk/highway.bst

depends:
- freedesktop-sdk.bst:components/brotli.bst

variables:
  cmake-local: |
    -DBUILD_TESTING=OFF \
    -DJPEGXL_ENABLE_FUZZERS=OFF \
    -DJPEGXL_ENABLE_DEVTOOLS=OFF \
    -DJPEGXL_ENABLE_TOOLS=OFF \
    -DJPEGXL_ENABLE_MANPAGES=OFF \
    -DJPEGXL_ENABLE_BENCHMARK=OFF \
    -DJPEGXL_ENABLE_EXAMPLES=OFF \
    -DJPEGXL_ENABLE_JNI=OFF \
    -DJPEGXL_ENABLE_VIEWERS=OFF \
    -DJPEGXL_ENABLE_TCMALLOC=OFF \
    -DJPEGXL_ENABLE_PLUGINS=OFF \
    -DJPEGXL_ENABLE_COVERAGE=OFF \
    -DJPEGXL_ENABLE_PROFILER=OFF \
    -DJPEGXL_ENABLE_TRANSCODE_JPEG=OFF \
    -DJPEGXL_ENABLE_SJPEG=OFF \
    -DJPEGXL_STATIC=OFF \
    -DJPEGXL_WARNINGS_AS_ERRORS=OFF \
    -DJPEGXL_ENABLE_SKCMS=ON \
    -DJPEGXL_FORCE_SYSTEM_BROTLI=ON \
    -DJPEGXL_FORCE_SYSTEM_GTEST=ON \
    -DJPEGXL_FORCE_SYSTEM_HWY=ON

config:
  # libhwy is used only internally and linked statically, yet the
  # generated .pc files will wrongly list it as a requirement.
  # Append a build command that edits the files to reflect reality.
  build-commands:
    (>):
      - |
        find '%{build-dir}' -name '*.pc' -print -execdir \
          sed -i -e '/^Requires\.private:/s/libhwy[[:space:]]*//' '{}' ';'

sources:
- kind: tar
  url: https://github.com/libjxl/libjxl/archive/refs/tags/v0.6.1.tar.gz
  ref: ccbd5a729d730152303be399f033b905e608309d5802d77a61a95faa092592c5
- kind: remote
  url: https://github.com/lvandeve/lodepng/raw/8c6a9e30576f07bf470ad6f09458a2dcd7a6a84a/lodepng.h
  ref: e6cb1736b11217209873f02b8859256e662e14d297f1f9346fbf1778d5efadbb
  directory: third_party/lodepng
- kind: remote
  url: https://github.com/lvandeve/lodepng/raw/8c6a9e30576f07bf470ad6f09458a2dcd7a6a84a/lodepng.cpp
  ref: 6c61ad12196fde3d2cbb2e5ce689db414ae7f64c98f709b227928b451280ed1b
  directory: third_party/lodepng
- kind: remote
  url: https://github.com/lvandeve/lodepng/raw/8c6a9e30576f07bf470ad6f09458a2dcd7a6a84a/LICENSE
  ref: bdafa318b8e637d600ca1b60545e894881977a16a16b74809c95e805701621e3
  directory: third_party/lodepng
- kind: git
  url: https://skia.googlesource.com/skcms
  ref: 64374756e03700d649f897dbd98c95e78c30c7da
  directory: third_party/skcms
  checkout-submodules: False
