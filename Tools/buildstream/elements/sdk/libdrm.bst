kind: meson
description: libdrm

depends:
- filename: freedesktop-sdk.bst:bootstrap-import.bst
- filename: freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
  type: build
- filename: freedesktop-sdk.bst:components/xorg-lib-pciaccess.bst

public:
  bst:
    overlap-whitelist:
    - '**'
    split-rules:
      devel:
        (>):
        - '%{libdir}/libdrm.so'
        - '%{libdir}/libkms.so'

sources:
- kind: git_tag
  url: gitlab_freedesktop_org:mesa/drm.git
  track: master
  ref: libdrm-2.4.102-0-gbb70ab82fd599b0a0db3d22354f05ae87fbc4515
