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
- kind: git_tag
  url: github_com:Sparkle-CDM/sparkle-cdm.git
  track: main
  track-tags: false
  ref: 72c4035f10fd4e4a91d3fde53465b33427f8fe0a
