kind: meson
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
- freedesktop-sdk.bst:components/nasm.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: git
  url: github_com:cisco/openh264.git
  track: master
  ref: 2e637867315ffeda3cd8970825ec86acc3fc4a30
variables:
  meson-local: >-
    -Dtests=disabled

public:
  bst:
    overlap-whitelist:
    - '**'
