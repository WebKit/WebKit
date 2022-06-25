kind: flatpak_image

build-depends:
- flatpak/sdk-image.bst

config:
  directory: '%{libdir}'
  exclude:
  - debug

  metadata:
    Runtime:
      name: org.webkit.Sdk.Compat.%{gcc_arch}
    ExtensionOf:
      ref: runtime/org.webkit.Sdk/%{gcc_arch}/%{sdk-branch}
