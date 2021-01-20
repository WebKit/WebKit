kind: flatpak_image

build-depends:
- flatpak/sdk-image.bst

config:
  directory: '%{datadir}/runtime/docs'
  metadata:
    Runtime:
      name: org.webkit.Sdk.Docs

    ExtensionOf:
      ref: runtime/org.webkit.Sdk/%{gcc_arch}/%{sdk-branch}
