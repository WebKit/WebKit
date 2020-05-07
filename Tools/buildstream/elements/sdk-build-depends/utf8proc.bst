kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: git_tag
  url: github_com:JuliaStrings/utf8proc
  track: v2.5.0
  ref: v2.5.0-0-g0890a538bf8238cded9be0c81171f57e43f2c755
