kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst
- freedesktop-sdk.bst:components/wayland-protocols.bst
- freedesktop-sdk.bst:components/xorg-proto-xorgproto.bst
- freedesktop-sdk.bst:components/xorg-util-macros.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/nettle.bst
- freedesktop-sdk.bst:components/libtirpc.bst
- freedesktop-sdk.bst:components/libdrm.bst
- freedesktop-sdk.bst:components/dummy-gbm.bst
- freedesktop-sdk.bst:components/libepoxy.bst
- freedesktop-sdk.bst:components/pixman.bst
- freedesktop-sdk.bst:components/xorg-lib-xkbfile.bst
- freedesktop-sdk.bst:components/xorg-lib-xfont2.bst
- freedesktop-sdk.bst:components/xorg-lib-xdmcp.bst
- freedesktop-sdk.bst:components/xorg-lib-xshmfence.bst
- freedesktop-sdk.bst:components/xorg-app-xkbcomp.bst
- freedesktop-sdk.bst:components/xorg-font-util.bst
- freedesktop-sdk.bst:components/wayland.bst
- freedesktop-sdk.bst:components/systemd.bst

variables:
  conf-local: |
     --disable-selective-werror \
     --disable-xinerama \
     --enable-glx \
     --enable-composite \
     --disable-xorg \
     --disable-dri3 \
     --disable-dmx \
     --disable-xnest \
     --disable-xquartz \
     --disable-xwin \
     --disable-xephyr \
     --disable-xfake \
     --disable-xfbdev \
     --disable-install-setuid \
     --disable-unit-tests \
     --disable-present \
     --enable-unix-transport \
     --enable-tcp-transport \
     --disable-local-transport \
     --with-xkb-path=/usr/share/X11/xkb \
     --with-xkb-output=/var/lib/xkb \
     --with-xkb-bin-directory=/usr/bin \
     --without-dtrace

sources:
- kind: tar
  url: https://www.x.org/releases/individual/xserver/xorg-server-1.20.8.tar.bz2
  ref: d17b646bee4ba0fb7850c1cc55b18e3e8513ed5c02bdf38da7e107f84e2d0146
