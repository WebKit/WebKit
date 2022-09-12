kind: cmake
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- sdk/capnp.bst

variables:
  cmake-local: |
    -Ddisable32bit=ON \
    -DBUILD_TESTS=OFF
sources:
- kind: git_tag
  url: github_com:rr-debugger/rr
  track: master
  ref: 5.6.0-12-g452f652321f87722da64ca363c3deea568ea0b67
- kind: patch
  path: patches/rr/0001-cmake-Look-for-resources-in-cmake-s-install-lib-dir.patch
