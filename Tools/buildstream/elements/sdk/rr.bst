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
  ref: 5.4.0-19-g7a520b7beea74a051abb5bf020155148264857ab
