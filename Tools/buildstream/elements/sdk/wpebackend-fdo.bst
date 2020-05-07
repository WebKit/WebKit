kind: cmake
sources:
- kind: git_tag
  url: github_com:Igalia/WPEBackend-fdo.git
  track: master
  ref: 1.3.1-54-g1f58ebdf632fdf90f691949d90c8338a8d7073c7
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst
depends:
- sdk/glib.bst
- sdk/libwpe.bst
- freedesktop-sdk.bst:components/mesa-headers.bst
- freedesktop-sdk.bst:components/wayland.bst
- freedesktop-sdk.bst:components/libepoxy.bst
- freedesktop-sdk.bst:bootstrap-import.bst
