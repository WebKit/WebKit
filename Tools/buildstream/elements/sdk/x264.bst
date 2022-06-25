kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: git_tag
  url: vlc:videolan/x264
  track: master
  ref: ae03d92b52bb7581df2e75d571989cb1ecd19cbd
variables:
  conf-local: >-
    --enable-shared
    --enable-static
    --enable-pic
    --disable-lavf
    --disable-asm
