kind: meson
sources:
- kind: tar
  url: gnome_downloads:glib/2.67/glib-2.67.4.tar.xz
  ref: 8d87b962032dadfcae8df62d248aa91fed2c7a43faf2c6d8b9107eb6c50e5b14
depends:
- filename: components/libffi.bst
  junction: freedesktop-sdk.bst
- filename: components/util-linux.bst
  junction: freedesktop-sdk.bst
- filename: components/python3.bst
  junction: freedesktop-sdk.bst
# a dummy dependency so our glib is always staged
# on top of freedesktop-sdk version
- filename: components/glib.bst
  junction: freedesktop-sdk.bst
  type: runtime
- filename: public-stacks/buildsystem-meson.bst
  junction: freedesktop-sdk.bst
  type: build
- filename: bootstrap-import.bst
  junction: freedesktop-sdk.bst
variables:
  meson-local: >-
    -Dgtk_doc=false
    -Dinstalled-tests=true
    -Dselinux=disabled
    -Dinternal_pcre=false
public:
  bst:
    integration-commands:
    - glib-compile-schemas %{prefix}/share/glib-2.0/schemas
    overlap-whitelist:
    - '**'
    split-rules:
      devel:
        (>):
        - '%{libdir}/libgio-2.0.so'
        - '%{libdir}/libglib-2.0.so'
        - '%{libdir}/libgmodule-2.0.so'
        - '%{libdir}/libgobject-2.0.so'
        - '%{libdir}/libgthread-2.0.so'
