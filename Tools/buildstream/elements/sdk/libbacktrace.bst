kind: autotools

sources:
- kind: git_repo
  url: https://github.com/ianlancetaylor/libbacktrace
  track: master
  ref: 9ae4f4ae4481b1e69d38ed810980d33103544613

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
