kind: meson

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- sdk/lzo2.bst

sources:
- kind: git_tag
  url: github_com:facebook/zstd
  ref: v1.4.4

variables:
  build-dir: build/meson
  meson-local: >-
    -Dlz4=disabled
  ninja:
    ninja -j ${NINJAJOBS}
  ninja-install:
    env DESTDIR="%{install-root}" ninja install
