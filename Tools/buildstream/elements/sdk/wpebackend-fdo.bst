kind: meson
sources:
- kind: git_tag
  url: github_com:Igalia/WPEBackend-fdo.git
  track: master
  ref: 1.7.1-3-g409af4493562c49196da5c9eace914ae10cfe9a3
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- sdk/glib.bst
- sdk/libwpe.bst
- freedesktop-sdk.bst:components/mesa-headers.bst
- freedesktop-sdk.bst:components/wayland.bst
- freedesktop-sdk.bst:components/libepoxy.bst
- freedesktop-sdk.bst:bootstrap-import.bst
