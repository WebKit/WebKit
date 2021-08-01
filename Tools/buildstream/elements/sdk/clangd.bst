kind: manual
sources:
- kind: zip
  url: github_files:clangd/clangd/releases/download/12.0.0/clangd-linux-12.0.0.zip
  ref: f9d09eb9f4f34cde78bdbcf9283a887734828c9893943abebe014a5630bce4d3

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
