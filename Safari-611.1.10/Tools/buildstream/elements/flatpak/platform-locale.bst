kind: flatpak_image

build-depends:
- flatpak/platform-image.bst

config:
  directory: '%{datadir}/runtime/locale'
  metadata:
    Runtime:
      name: org.webkit.Platform.Locale

    ExtensionOf:
      ref: runtime/org.webkit.Platform/%{gcc_arch}/%{sdk-branch}
