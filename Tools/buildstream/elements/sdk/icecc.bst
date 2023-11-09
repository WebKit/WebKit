kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/zstd.bst
- freedesktop-sdk.bst:components/libcap-ng.bst
- freedesktop-sdk.bst:components/libarchive.bst
- freedesktop-sdk.bst:components/lzo.bst
- freedesktop-sdk.bst:components/openssl.bst

variables:
  conf-local: >-
    --without-man

sources:
- kind: git_repo
  url: github_com:icecc/icecream
  track: master
  ref: 1.4rc1-44-g9d397ef13508053802fb5048a92e92b918e5c32e
