kind: compose

build-depends:
- freedesktop-sdk.bst:integration/debug-app-link.bst
- freedesktop-sdk.bst:integration/flatpak-builder-configuration.bst
- freedesktop-sdk.bst:integration/split-docs.bst
- freedesktop-sdk.bst:integration/split-locales.bst
- freedesktop-sdk.bst:multiarch/multiarch-sdk.bst
- sdk.bst
- manifests/sdk-manifest.bst

config:
  exclude:
  - extra
  - static-blocklist
  - vm-only

public:
  bst:
    split-rules:
      docs:
      - '%{datadir}/runtime/docs/**'
      locale:
      - '%{datadir}/runtime/locale/**'
