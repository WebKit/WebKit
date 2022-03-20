kind: meson
sources:
- kind: tar
  url: gst_downloads:gstreamer/gstreamer-1.20.1.tar.xz
  ref: de094a404a3ad8f4977829ea87edf695a4da0b5c8f613ebe54ab414bac89f031
# Upstreamed patches most likely shipping in 1.20.2.
- kind: patch
  path: patches/gstreamer-0001-typefind-Skip-parsing-of-data-URIs.patch
- kind: patch
  path: patches/gstreamer-0002-uri-Build-doubly-linked-list-by-prepending-items.patch
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
