kind: meson
sources:
- kind: git_repo
  url: github_com:Igalia/WPEBackend-fdo.git
  track: 1.12.0
  ref: 1.12.0-0-g1b35ae0d8f32e32431da494d3389c5b1a8812cab
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- freedesktop-sdk.bst:components/glib.bst
- sdk/libwpe.bst
- freedesktop-sdk.bst:components/mesa-headers.bst
- freedesktop-sdk.bst:components/wayland.bst
- freedesktop-sdk.bst:components/libepoxy.bst
- freedesktop-sdk.bst:bootstrap-import.bst
