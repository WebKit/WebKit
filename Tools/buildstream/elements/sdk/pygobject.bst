kind: distutils
sources:
- kind: tar
  url: gnome_downloads:pygobject/3.36/pygobject-3.36.0.tar.xz
  ref: 8683d2dfb5baa9e501a9a64eeba5c2c1117eadb781ab1cd7a9d255834af6daef
build-depends:
- freedesktop-sdk.bst:components/pkg-config.bst
depends:
- sdk/glib.bst
- freedesktop-sdk.bst:components/gobject-introspection.bst
- sdk/pycairo.bst
- freedesktop-sdk.bst:bootstrap-import.bst
