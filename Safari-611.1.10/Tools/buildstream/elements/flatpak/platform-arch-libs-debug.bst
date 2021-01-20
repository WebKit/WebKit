kind: flatpak_image

build-depends:
- flatpak/platform-image.bst

config:
  directory: '%{debugdir}%{libdir}'
  include:
  - debug

  metadata:
    Runtime:
      name: org.webkit.Platform.Compat.%{gcc_arch}.Debug
    ExtensionOf:
      ref: runtime/org.webkit.Platform/%{gcc_arch}/%{sdk-branch}

