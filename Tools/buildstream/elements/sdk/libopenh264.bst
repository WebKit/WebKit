kind: meson
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
- freedesktop-sdk.bst:components/nasm.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: git_repo
  url: github_com:cisco/openh264.git
  track: master
  ref: v2.5.0-8-g423eb2c3e47009f4e631b5e413123a003fdff1ed
variables:
  meson-local: >-
    -Dtests=disabled

public:
  bst:
    overlap-whitelist:
    - '**'
