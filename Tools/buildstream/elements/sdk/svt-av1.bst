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
- kind: git_tag
  url: gitlab_com:AOMediaCodec/SVT-AV1
  track: v*.*.*
  ref: v1.5.0-0-gea296ef350714fb6f105b420fb0bc321d9997ffd
