kind: meson
sources:
- kind: tar
  url: gnome_downloads:libsecret/0.20/libsecret-0.20.2.tar.xz
  ref: 81e9143833785cdcf96c1da5d0357a8bcf0cd2b0119f15aa0cae775d1f19ffc3
- kind: patch
  path: patches/libsecret-b53e9876f4611525b7a8f69a3110fbec51dcede0.patch
build-depends:
- freedesktop-sdk.bst:components/valgrind.bst
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
- freedesktop-sdk.bst:components/libxslt.bst
depends:
- sdk/glib.bst
- sdk/gobject-introspection.bst
- freedesktop-sdk.bst:components/libgcrypt.bst
- freedesktop-sdk.bst:bootstrap-import.bst
variables:
  meson-local: >-
    -Dgtk_doc=false
    -Dvapi=false
    -Dmanpage=false
public:
  bst:
    split-rules:
      devel:
        (>):
        - '%{libdir}/libsecret-1.so'
