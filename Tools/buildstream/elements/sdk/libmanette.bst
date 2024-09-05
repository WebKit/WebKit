kind: meson
sources:
- kind: git_repo
  url: gitlab_gnome_org:GNOME/libmanette.git
  track: 0.2.*
  ref: 0.2.7-0-g27554bd118c08141134d4147cc34794057eb36e2

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst

depends:
- freedesktop-sdk.bst:components/glib.bst
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/libevdev.bst

variables:
  meson-local: >-
    -Dvapi=false
    -Dintrospection=false
    -Dgudev=disabled
