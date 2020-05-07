kind: flatpak_image

build-depends:
- flatpak/sdk-image.bst

config:
  directory: '%{debugdir}'
  metadata:
    Runtime:
      name: org.webkit.Sdk.Debug

    ExtensionOf:
      ref: runtime/org.webkit.Sdk/%{gcc_arch}/%{sdk-branch}

