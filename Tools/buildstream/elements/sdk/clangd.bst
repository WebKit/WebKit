kind: manual
sources:
- kind: zip
  url: github_files:clangd/clangd/releases/download/14.0.0/clangd-linux-14.0.0.zip
  ref: 5bff46c43c07e7de368df0a4d39aae51fa42db1ec5464fc9e5bb82b1bba45552
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
