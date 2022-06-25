kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/zstd.bst
- sdk/libcap-ng.bst
- freedesktop-sdk.bst:components/libarchive.bst
- freedesktop-sdk.bst:components/lzo.bst

variables:
  conf-local: >-
    --without-man

sources:
- kind: git_tag
  url: github_com:icecc/icecream
  ref: 1.3.1

