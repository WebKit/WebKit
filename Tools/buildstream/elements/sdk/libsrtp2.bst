kind: autotools
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: git
  url: github_com:cisco/libsrtp.git
  ref: d02d21111e379c297e93a9033d7b653135f732ee

