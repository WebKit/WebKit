kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: tar
  url: https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/libkate/libkate-0.4.1.tar.gz
  ref: c40e81d5866c3d4bf744e76ce0068d8f388f0e25f7e258ce0c8e76d7adc87b68
