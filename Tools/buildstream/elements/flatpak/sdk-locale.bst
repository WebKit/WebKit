kind: flatpak_image

build-depends:
- flatpak/sdk-image.bst

config:
  directory: '%{datadir}/runtime/locale'
  metadata:
    Runtime:
      name: org.webkit.Sdk.Locale

    ExtensionOf:
      ref: runtime/org.webkit.Sdk/%{gcc_arch}/%{sdk-branch}
