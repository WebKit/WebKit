kind: meson
sources:
- kind: tar
  url: gnome_downloads:graphene/1.10/graphene-1.10.0.tar.xz
  ref: 406d97f51dd4ca61e91f84666a00c3e976d3e667cd248b76d92fdb35ce876499
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst
depends:
- sdk/glib.bst
- freedesktop-sdk.bst:components/gobject-introspection.bst
- freedesktop-sdk.bst:bootstrap-import.bst
public:
  bst:
    overlap-whitelist:
    - '**'
    split-rules:
      devel:
        (>):
        - '%{libdir}/libgraphene-1.0.so'
