kind: manual
sources:
- kind: tar
  url: github_files:arduino/clang-static-binaries/releases/download/15.0.0/clangd_15.0.0_Linux_ARM64.tar.bz2
  ref: 176b60b321135114c4f063be82ec0f325bc3a9f8b5b411050ceda1675d971cfe
depends:
- freedesktop-sdk.bst:bootstrap-import.bst

config:
  install-commands:
  - |
    install -D -m a+rx -t "%{install-root}/%{bindir}" ./clangd

variables:
  strip-binaries: "true"
public:
  bst:
    integration-commands:
    - |
      clangd --version
