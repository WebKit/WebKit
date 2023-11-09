kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/alsa-lib.bst

sources:
- kind: git_repo
  url: github_com:festvox/flite
  track: master
  ref: v2.2-45-g6c9f20dc915b17f5619340069889db0aa007fcdc
- kind: patch
  path: patches/flite-fix-voice-list-build.patch

variables:
  conf-local: |
    --enable-shared
  make: |
    make -j1
