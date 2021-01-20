kind: pip

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- sdk/python2.bst
- sdk/glib.bst
- freedesktop-sdk.bst:components/gobject-introspection.bst
- sdk/python2-pycairo.bst

build-depends:
- freedesktop-sdk.bst:components/pkg-config.bst

variables:
  pip: pip2

sources:
- kind: tar
  url: gnome_downloads:pygobject/3.28/pygobject-3.28.1.tar.xz
  ref: 42312b4a5015571fa0a4f2d201005da46b71c251ea2625bc95702d071c4ff895

public:
  bst:
    overlap-whitelist:
    - '**'

