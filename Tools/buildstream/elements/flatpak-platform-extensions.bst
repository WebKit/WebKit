kind: flatpak_repo

build-depends:
- freedesktop-sdk.bst:components/flatpak.bst
- filename:
  - freedesktop-sdk.bst:flatpak-images/mesa.bst
  config:
    flatpak-image: true

(?):
- arch in ("i686", "x86_64"):
    build-depends:
      (>):
      - filename:
        - freedesktop-sdk.bst:flatpak-images/platform-vaapi-intel.bst
        config:
          flatpak-image: true

config:
  arch: '%{gcc_arch}'
  branch: '%{sdk-branch}'
  repo-mode: bare-user-only
