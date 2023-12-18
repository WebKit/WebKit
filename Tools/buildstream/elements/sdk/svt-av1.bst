kind: cmake

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst
- freedesktop-sdk.bst:components/nasm.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

variables:
  cmake-local: >-
    -DREPRODUCIBLE_BUILDS=1

public:
  bst:
    split-rules:
      devel:
        (>):
        - "%{libdir}/libSvtAv1Dec.so"
        - "%{libdir}/libSvtAv1Enc.so"

sources:
- kind: git_repo
  url: gitlab_com:AOMediaCodec/SVT-AV1
  track: v*.*.*
  ref: v1.7.0-0-g6d5f2b4e1f04db677cdd4ab2b0d6831ab06a849c
