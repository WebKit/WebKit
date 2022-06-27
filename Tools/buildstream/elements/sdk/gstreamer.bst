kind: meson
sources:
- kind: tar
  url: gst_downloads:gstreamer/gstreamer-1.20.3.tar.xz
  ref: 607daf64bbbd5fb18af9d17e21c0d22c4d702fffe83b23cb22d1b1af2ca23a2a
build-depends:
- freedesktop-sdk.bst:components/gobject-introspection.bst
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
- freedesktop-sdk.bst:components/flex.bst
- freedesktop-sdk.bst:components/bison.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/glib.bst
- freedesktop-sdk.bst:components/libunwind.bst

variables:
  libexecdir: "%{libdir}"
  meson-local: >-
    -Dexamples=disabled
    -Dbenchmarks=disabled
    -Dintrospection=enabled
    -Dlibunwind=enabled
    -Dlibdw=enabled
    -Ddbghelp=disabled
    -Dbash-completion=disabled
    -Ddoc=disabled
    -Dpackage-origin="webkit-sdk"
public:
  bst:
    overlap-whitelist:
    - '**'
    split-rules:
      devel:
        (>):
        - '%{libdir}/libgstcheck-1.0.so'
        - '%{libdir}/libgstbase-1.0.so'
        - '%{libdir}/libgstnet-1.0.so'
        - '%{libdir}/libgstreamer-1.0.so'
        - '%{libdir}/libgstcontroller-1.0.so'
