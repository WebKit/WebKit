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
  url: github_com:mozilla/rr
  track: 5.3.0
  ref: 5.3.0-0-g47f13a8ce92fd1dfba45bd4f9faed8f0b202f4ee
