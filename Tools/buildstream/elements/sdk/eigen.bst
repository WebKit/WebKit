kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: git_tag
  url: gitlab_com:libeigen/eigen
  track: '3.3'
  ref: 3.3.9-1-ga36d19c4fc46097da9dd9965e7d548aa05d2cbf8
