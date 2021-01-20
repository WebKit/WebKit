kind: meson
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
- freedesktop-sdk.bst:components/nasm.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: git
  url: github_com:cisco/openh264.git
  ref: 2122fe08cc8fe69899c3e257d0ee030c28a64174
variables:
  meson-local: >-
    -Dtests=disabled

public:
  bst:
    overlap-whitelist:
    - '**'
