kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst
- sdk-build-depends/utf8proc.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/sqlite.bst
- freedesktop-sdk.bst:components/expat.bst
- test-infra/apr.bst
- test-infra/apr-util.bst

sources:
- kind: tar
  url: http://apache.uvigo.es/subversion/subversion-1.13.0.tar.bz2
  ref: bc50ce2c3faa7b1ae9103c432017df98dfd989c4239f9f8270bb3a314ed9e5bd
variables:
  conf-local: >-
    --with-apr=/usr/bin/apr-1-config
    --with-apr-util=/usr/bin/apu-1-config
    --with-lz4=internal
