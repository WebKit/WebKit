kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst

sources:
- kind: git
  url: https://chromium.googlesource.com/breakpad/breakpad
  ref: d8a7c0548c7b30689df9a65e52b7ab9db37ab9e8
  track: chrome_99
- kind: git
  url: https://chromium.googlesource.com/linux-syscall-support
  ref: e1e7b0ad8ee99a875b272c8e33e308472e897660
  track: main
  directory: src/third_party/lss

public:
  bst:
    integration-commands:
    - |
      minidump-2-core -h
