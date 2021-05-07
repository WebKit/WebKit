kind: meson
sources:
- kind: git_tag
  url: github_com:Igalia/WPEBackend-fdo.git
  track: 1.8.3
  ref: 1.8.3-0-gd21c34e4d844dd89685ea9f8da3ec5ebe147cf12
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- sdk/glib.bst
- sdk/libwpe.bst
- freedesktop-sdk.bst:components/mesa-headers.bst
- freedesktop-sdk.bst:components/wayland.bst
- freedesktop-sdk.bst:components/libepoxy.bst
- freedesktop-sdk.bst:bootstrap-import.bst
