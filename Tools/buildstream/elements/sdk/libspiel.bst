kind: meson

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/gstreamer-plugins-base.bst

variables:
  meson-local: >-
    -Dtests=false
    -Ddocs=false
    -Dlibspeechprovider:docs=false
    -Dlibspeechprovider:introspection=false

sources:
- kind: git_repo
  url: github_com:project-spiel/libspiel.git
  track: main
  ref: SPIEL_1_0_1-31-gac56d0dd19b2b2ecd9e81a3c436cc9f9e7299e9d
- kind: git_repo
  url: github_com:project-spiel/libspeechprovider.git
  track: main
  ref: SPEECHPROVIDER_1_0_1-11-g3de6db3b18b8da26f2d60c35c330ff68be96e992
  directory: subprojects/libspeechprovider
