kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/alsa-lib.bst

sources:
- kind: tar
  url: github_com:festvox/flite/archive/refs/tags/v2.2.tar.gz
  ref: ab1555fe5adc3f99f1d4a1a0eb1596d329fd6d74f1464a0097c81f53c0cf9e5c

variables:
  conf-local: |
    --enable-shared
