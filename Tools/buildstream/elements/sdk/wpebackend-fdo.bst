kind: meson
sources:
- kind: git_tag
  url: github_com:Igalia/WPEBackend-fdo.git
  track: 1.10.0
  ref: 1.10.0-0-ga908c630483bc99d0c87d4086af183c9a4cafde2
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- sdk/glib.bst
- sdk/libwpe.bst
- freedesktop-sdk.bst:components/mesa-headers.bst
- freedesktop-sdk.bst:components/wayland.bst
- freedesktop-sdk.bst:components/libepoxy.bst
- freedesktop-sdk.bst:bootstrap-import.bst
