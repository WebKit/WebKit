kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: git_repo
  url: gitlab_com:libeigen/eigen
  track: 3.4.*
  ref: 3.4.0-0-g3147391d946bb4b6c68edd901f2add6ac1f31f8c
