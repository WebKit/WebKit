kind: manual
sources:
- kind: zip
  url: github_files:clangd/clangd/releases/download/14.0.3/clangd-linux-14.0.3.zip
  ref: 6c6eabfcc5ce91dea2ebce039cf0dea612a191ebbbffe366d973f0a102325bce
depends:
- freedesktop-sdk.bst:bootstrap-import.bst

config:
  install-commands:
  - |
    install -D -m a+rx -t "%{install-root}/%{bindir}" ./bin/clangd

variables:
  strip-binaries: "true"
public:
  bst:
    integration-commands:
    - |
      clangd --version
