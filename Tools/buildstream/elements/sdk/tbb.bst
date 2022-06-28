kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- sdk/hwloc.bst

variables:
  cmake-local: >-
    -DTBB4PY_BUILD=OFF
    -DTBBMALLOC_BUILD=OFF
    -DTBB_BENCH=OFF
    -DTBB_BUILD=ON
    -DTBB_CPF=OFF
    -DTBB_DISABLE_HWLOC_AUTOMATIC_SEARCH=OFF
    -DTBB_EXAMPLES=OFF
    -DTBB_FIND_PACKAGE=OFF
    -DTBB_STRICT=OFF
    -DTBB_TEST=OFF
    -DBUILD_SHARED_LIBS=ON

sources:
- kind: git_tag
  url: github_com:oneapi-src/oneTBB.git
  checkout-submodules: false
  track-tags: true
  track: master
  ref: v2021.5.0-g3df08fe234f23e732a122809b40eb129ae22733f
- kind: patch
  path: patches/tbb-Retry-if-pthread_create-fails-with-EAGAIN.patch
