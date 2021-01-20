kind: manual
sources:
- kind: zip
  url: github_files:clangd/clangd/releases/download/11.0.0/clangd-linux-11.0.0.zip
  ref: 10047e2f974cd35a145e672eb5747b6cb27902181ae78c148d6e697a9da562d8

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
