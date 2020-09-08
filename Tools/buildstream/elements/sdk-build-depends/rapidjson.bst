kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: git_tag
  url: github_com:Tencent/rapidjson
  track: master
  ref: v1.1.0-565-gce81bc9edfe773667a7a4454ba81dac72ed4364c
- kind: patch
  path: patches/rapidjson-0001-Fix-build-warnings-emitted-by-GCC-10-on-Aarch64.patch
