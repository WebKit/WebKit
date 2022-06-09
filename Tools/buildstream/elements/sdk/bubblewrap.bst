kind: autotools
sources:
- kind: tar
  url: github_com:containers/bubblewrap/releases/download/v0.5.0/bubblewrap-0.5.0.tar.xz
  ref: 16fdaf33799d63104e347e0133f909196fe90d0c50515d010bcb422eb5a00818
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

depends:
- freedesktop-sdk.bst:components/libcap.bst
- freedesktop-sdk.bst:bootstrap-import.bst
