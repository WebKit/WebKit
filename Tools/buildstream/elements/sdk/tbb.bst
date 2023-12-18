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
- kind: git_repo
  url: github_com:oneapi-src/oneTBB.git
  track: v2021.*
  ref: v2021.11.0-src-rc1-0-gcb6cc0ec55353b7f48b19c45586f4736b89c4eac
