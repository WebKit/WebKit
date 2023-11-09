kind: autotools
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: git_repo
  url: github_com:NixOS/patchelf.git
  track: '0.18.0'
  ref: 0.18.0-0-g99c24238981b7b1084313aca8f5c493bb46f302c
