kind: flatpak_image

build-depends:
- flatpak/platform-image.bst
- freedesktop-sdk.bst:integration/compat-gl-dir.bst

config:
  directory: '%{libdir}'
  exclude:
  - debug

  metadata:
    Runtime:
      name: org.webkit.Platform.Compat.%{gcc_arch}
    ExtensionOf:
      ref: runtime/org.webkit.Platform/%{gcc_arch}/%{sdk-branch}

