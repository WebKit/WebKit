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
  ref: 5.5.0-5-gc168a271dd5f3685834a74e5d96f60cc89a9f6c0
