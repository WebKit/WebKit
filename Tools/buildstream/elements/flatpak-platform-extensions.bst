kind: flatpak_repo

build-depends:
- freedesktop-sdk.bst:components/flatpak.bst
- freedesktop-sdk.bst:flatpak-images/mesa.bst

(?):
- arch in ("i686", "x86_64"):
    build-depends:
      (>):
      - freedesktop-sdk.bst:flatpak-images/platform-vaapi-intel.bst

config:
  environment:
  - components/flatpak.bst
  arch: '%{gcc_arch}'
  branch: '%{sdk-branch}'
  repo-mode: bare-user-only
