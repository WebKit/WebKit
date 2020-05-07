kind: autotools
sources:
- kind: tar
  url: https://linuxtv.org/downloads/v4l-utils/v4l-utils-1.18.0.tar.bz2
  ref: 6cb60d822eeed20486a03cc23e0fc65956fbc1e85e0c1a7477f68bbd9802880d
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst
depends:
- freedesktop-sdk.bst:bootstrap-import.bst
