kind: meson

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/json-glib.bst
- freedesktop-sdk.bst:components/gstreamer.bst

variables:
  meson-local: >-
    -Dsample-player=disabled

sources:
- kind: git_repo
  url: github_com:Sparkle-CDM/sparkle-cdm.git
  track: main
  ref: 3e1e0dcb221d044213f8c6e32b0b06102e7fb224
