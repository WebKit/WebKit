kind: meson
sources:
- kind: git_tag
  url: github_com:Igalia/WPEBackend-fdo.git
  track: 1.8.4
  ref: 1.8.4-0-gfe0665f101f40ef51b8b075c03cf45b5dfcfed08
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- sdk/glib.bst
- sdk/libwpe.bst
- freedesktop-sdk.bst:components/mesa-headers.bst
- freedesktop-sdk.bst:components/wayland.bst
- freedesktop-sdk.bst:components/libepoxy.bst
- freedesktop-sdk.bst:bootstrap-import.bst
