kind: cmake
sources:
- kind: git_tag
  url: github_com:Igalia/WPEBackend-fdo.git
  track: master
  ref: 1.3.1-68-g343146b98c365306f3718d50a4a52537de5e3dcc
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst
depends:
- sdk/glib.bst
- sdk/libwpe.bst
- freedesktop-sdk.bst:components/mesa-headers.bst
- freedesktop-sdk.bst:components/wayland.bst
- freedesktop-sdk.bst:components/libepoxy.bst
- freedesktop-sdk.bst:bootstrap-import.bst
