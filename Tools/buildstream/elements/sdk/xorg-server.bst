kind: meson

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
- freedesktop-sdk.bst:components/wayland-protocols.bst
- freedesktop-sdk.bst:components/xorg-proto-xorgproto.bst
- freedesktop-sdk.bst:components/xorg-util-macros.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/nettle.bst
- freedesktop-sdk.bst:components/libtirpc.bst
- freedesktop-sdk.bst:components/dummy-gbm.bst
- freedesktop-sdk.bst:components/libepoxy.bst
- freedesktop-sdk.bst:components/pixman.bst
- freedesktop-sdk.bst:components/xorg-lib-xcvt.bst
- freedesktop-sdk.bst:components/xorg-lib-xkbfile.bst
- freedesktop-sdk.bst:components/xorg-lib-xfont2.bst
- freedesktop-sdk.bst:components/xorg-lib-xdmcp.bst
- freedesktop-sdk.bst:components/xorg-lib-xshmfence.bst
- freedesktop-sdk.bst:components/xorg-app-xkbcomp.bst
- freedesktop-sdk.bst:components/xorg-font-util.bst
- freedesktop-sdk.bst:components/wayland.bst
- freedesktop-sdk.bst:components/systemd.bst
- freedesktop-sdk.bst:components/libdrm.bst

variables:
  meson-local: >-
    -Dxinerama=false
    -Dxorg=false
    -Ddri3=false
    -Dxnest=false
    -Dxquartz=false
    -Dxwin=false
    -Dxephyr=false

sources:
- kind: tar
  url: https://www.x.org/releases/individual/xserver/xorg-server-21.1.8.tar.xz
  ref: 38aadb735650c8024ee25211c190bf8aad844c5f59632761ab1ef4c4d5aeb152
