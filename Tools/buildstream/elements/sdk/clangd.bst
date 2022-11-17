kind: manual
sources:
- kind: zip
  url: github_files:clangd/clangd/releases/download/15.0.3/clangd-linux-15.0.3.zip
  ref: c1ebe5b37372553273703f16b7d00c7bc6cf42f4997a07110d18c2105932b9d6
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
