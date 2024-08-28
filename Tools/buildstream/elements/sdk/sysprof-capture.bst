kind: meson

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst

depends:
- sdk/libdex.bst

variables:
  meson-local: >-
    -Dgtk=false
    -Dsysprofd=host
    -Dhelp=false
    -Dlibsysprof=false
    -Dtools=false
    -Dtests=false
    -Dexamples=false

sources:
- kind: tar
  url: gnome_downloads:sysprof/46/sysprof-46.0.tar.xz
  ref: 73aa7e75ebab3e4e0946a05a723df7e6ee4249e3b9e884dba35500aba2a1d176

public:
  bst:
    integration-commands:
    - |
      test $(pkg-config --modversion sysprof-capture-4) = 46.0
