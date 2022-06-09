kind: meson
sources:
- kind: tar
  url: gnome_downloads:gtk+/3.24/gtk+-3.24.30.tar.xz
  ref: ba75bfff320ad1f4cfbee92ba813ec336322cc3c660d406aad014b07087a3ba9
- kind: patch
  path: patches/gtk-gcc11-build-fix.patch
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- freedesktop-sdk.bst:components/at-spi2-atk.bst
- freedesktop-sdk.bst:components/atk.bst
- freedesktop-sdk.bst:components/gdk-pixbuf.bst
- sdk/glib.bst
- freedesktop-sdk.bst:components/gobject-introspection.bst
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
