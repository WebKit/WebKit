kind: autotools
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: git_tag
  url: github_com:NixOS/patchelf.git
  track: '0.14.5'
  ref: 0.14.5-0-ga35054504293f9ff64539850d1ed0bfd2f5399f2
