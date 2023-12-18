kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

depends:
- freedesktop-sdk.bst:components/zstd.bst

sources:
- kind: git_repo
  url: https://chromium.googlesource.com/breakpad/breakpad
  ref: v2023.01.27-56-gf49c2f1a2023da0cb055874fba050563dfea57db
  track: main
- kind: git_repo
  url: https://chromium.googlesource.com/linux-syscall-support
  ref: v2022.10.12-0-g9719c1e1e676814c456b55f5f070eabad6709d31
  track: main
  directory: src/third_party/lss

public:
  bst:
    integration-commands:
    - |
      minidump-2-core -h
