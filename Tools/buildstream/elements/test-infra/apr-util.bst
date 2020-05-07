kind: autotools
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst
- freedesktop-sdk.bst:components/expat.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- test-infra/apr.bst
sources:
- kind: tar
  url: https://www.apache.org/dist/apr/apr-util-1.6.1.tar.bz2
  ref: d3e12f7b6ad12687572a3a39475545a072608f4ba03a6ce8a3778f607dd0035b

variables:
  conf-local: >-
    --with-apr=/usr/bin/apr-1-config
