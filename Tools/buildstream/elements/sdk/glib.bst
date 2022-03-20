kind: meson
sources:
- kind: tar
  url: gnome_downloads:glib/2.70/glib-2.70.0.tar.xz
  ref: 200d7df811c5ba634afbf109f14bb40ba7fde670e89389885da14e27c0840742
depends:
- filename: components/libffi.bst
  junction: freedesktop-sdk.bst
- filename: components/util-linux.bst
  junction: freedesktop-sdk.bst
- filename: components/python3.bst
  junction: freedesktop-sdk.bst
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
    -Dselinux=disabled
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
