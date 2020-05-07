kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: tar
  url: http://download.videolan.org/pub/x264/snapshots/x264-snapshot-20140212-2245-stable.tar.bz2
  ref: 5d98e9e4faf6dd55e7193ed379aff477b8acbda6777758956ef7e5f05067be18
variables:
  conf-local: >-
    --enable-shared
    --enable-static
    --enable-pic
    --disable-lavf
    --disable-asm
