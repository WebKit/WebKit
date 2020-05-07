kind: meson

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/gdk-pixbuf.bst

sources:
- kind: tar
  url: gnome_downloads:libnotify/0.7/libnotify-0.7.8.tar.xz
  ref: 69209e0b663776a00c7b6c0e560302a8dbf66b2551d55616304f240bba66e18c
variables:
  meson-local: >-
   -Dtests=false
   -Dintrospection=disabled
   -Dgtk_doc=false
   -Ddocbook_docs=disabled
