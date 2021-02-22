kind: meson
sources:
- kind: tar
  url: gst_downloads:gstreamer/gstreamer-1.18.3.tar.xz
  ref: 0c2e09e18f2df69a99b5cb3bd53c597b3cc2e35cf6c98043bb86a66f3d312100
# Backport of https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/679 to be shipped in 1.20.
- kind: patch
  path: patches/gstreamer-0001-devicemonitor-Stop-only-the-already-started-provider.patch

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
    -Dgtk_doc=disabled
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
