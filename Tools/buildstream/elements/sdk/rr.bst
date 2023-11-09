kind: cmake
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst
- freedesktop-sdk.bst:components/python3.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- sdk/capnp.bst

variables:
  cmake-local: |
    -Ddisable32bit=ON \
    -DBUILD_TESTS=OFF
sources:
- kind: git_repo
  url: github_com:rr-debugger/rr
  track: 5.*
  ref: 5.7.0-0-g7cf5a12dba5aaeea9efc5ad97176b07b5614e350
- kind: patch
  path: patches/rr/0001-cmake-Look-for-resources-in-cmake-s-install-lib-dir.patch
