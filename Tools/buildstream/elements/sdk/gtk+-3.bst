kind: meson
sources:
- kind: tar
  url: gnome_downloads:gtk+/3.24/gtk+-3.24.16.tar.xz
  ref: 0d5e1e1494101b8c0c63c0526180780559eee469f021ca0d714018b20fa3d8e8
- kind: patch
  path: patches/gtk-meson-Bump-to-latest-version-of-the-3.24-branch.patch
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- sdk/at-spi2-atk.bst
- sdk/atk.bst
- freedesktop-sdk.bst:components/gdk-pixbuf.bst
- sdk/glib.bst
- sdk/gobject-introspection.bst
- freedesktop-sdk.bst:components/pango.bst
- freedesktop-sdk.bst:components/cairo.bst
- freedesktop-sdk.bst:components/hicolor-icon-theme.bst
- freedesktop-sdk.bst:components/libepoxy.bst
- freedesktop-sdk.bst:components/libxkbcommon.bst
- freedesktop-sdk.bst:components/mesa-headers.bst
- freedesktop-sdk.bst:components/wayland-protocols.bst
- freedesktop-sdk.bst:components/wayland.bst
- freedesktop-sdk.bst:components/xorg-lib-xcomposite.bst
- freedesktop-sdk.bst:components/xorg-lib-xcursor.bst
- freedesktop-sdk.bst:components/xorg-lib-xdamage.bst
- freedesktop-sdk.bst:components/xorg-lib-xi.bst
- freedesktop-sdk.bst:components/xorg-lib-xinerama.bst
- freedesktop-sdk.bst:bootstrap-import.bst
variables:
  meson-local: >-
    -Dbroadway_backend=true
    -Dinstalled_tests=true
    -Dtests=false
    -Dx11_backend=true
    -Dwayland-backend=true
    -Dgtk_doc=false
    -Dman=false
    -Dintrospection=true
public:
  bst:
    integration-commands:
    - |-
      for dir in %{prefix}/share/icons/*; do
        if test -f $dir/index.theme; then
          gtk-update-icon-cache --quiet $dir
        fi
      done
    - gtk-query-immodules-3.0 --update-cache
    overlap-whitelist:
    - '**'
    split-rules:
      devel:
        (>):
        - '%{libdir}/libgailutil-3.so'
        - '%{libdir}/libgdk-3.so'
        - '%{libdir}/libgtk-3.so'
    cpe:
      product: gtk+
