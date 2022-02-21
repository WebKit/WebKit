kind: cmake
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/xorg-lib-x11.bst
- qt5/qtbase.bst

variables:
  cmake-local: |
    -DENABLE_GUI=TRUE

sources:
- kind: git_tag
  url: github_com:apitrace/apitrace
  track: '10.0'
  ref: 10.0-0-g03e4e9d2cab4f1a61d3f589785641a73dff027c5
