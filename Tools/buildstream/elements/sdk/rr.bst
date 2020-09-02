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
  track: master
  ref: 5.3.0-316-gcd61ba22ccc05b426691312784674c0eb8e654ef
