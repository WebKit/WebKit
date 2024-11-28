kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst
- sdk/highway.bst

depends:
- freedesktop-sdk.bst:components/brotli.bst
- freedesktop-sdk.bst:components/jpeg.bst
- freedesktop-sdk.bst:components/libpng.bst

variables:
  cmake-local: |
    -DBUILD_TESTING=OFF \
    -DJPEGXL_ENABLE_FUZZERS=OFF \
    -DJPEGXL_ENABLE_DEVTOOLS=OFF \
    -DJPEGXL_ENABLE_TOOLS=OFF \
    -DJPEGXL_ENABLE_MANPAGES=OFF \
    -DJPEGXL_ENABLE_BENCHMARK=OFF \
    -DJPEGXL_ENABLE_EXAMPLES=OFF \
    -DJPEGXL_ENABLE_JPEGLI=OFF \
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
    -DJPEGXL_FORCE_SYSTEM_HWY=ON \
    -DJPEGXL_BUNDLE_LIBPNG=OFF

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
  url: github_com:libjxl/libjxl/archive/refs/tags/v0.10.3.tar.gz
  ref: e0191411cfcd927eebe5392d030fe4283fe27ba1685ab7265104936e0b4283a6
- kind: git_repo
  url: https://skia.googlesource.com/skcms
  ref: 42030a771244ba67f86b1c1c76a6493f873c5f91
  directory: third_party/skcms

