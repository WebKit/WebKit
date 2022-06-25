kind: flatpak_image

build-depends:
- flatpak/sdk-image.bst

config:
  directory: '%{debugdir}%{libdir}'
  include:
  - debug

  metadata:
    Runtime:
      name: org.webkit.Sdk.Compat.%{gcc_arch}.Debug
    ExtensionOf:
      ref: runtime/org.webkit.Sdk/%{gcc_arch}/%{sdk-branch}
