kind: manual
sources:
- kind: zip
  url: github_files:clangd/clangd/releases/download/16.0.2/clangd-linux-16.0.2.zip
  ref: 928f826340e322e3eed534f4b215e5c403c22e73dbfa66ae4fd8f44e22f4ca28
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
