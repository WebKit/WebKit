kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst
- freedesktop-sdk.bst:components/ninja.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

variables:
  cmake-local: >-
    -DBUILD_SHARED_LIBS=OFF

sources:
- kind: git_repo
  url: github_com:fmtlib/fmt.git
  track: '*.*.*'
  ref: 11.2.0-0-g40626af88bd7df9a5fb80be7b25ac85b122d6c21
