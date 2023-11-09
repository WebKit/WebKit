kind: flatpak_repo

build-depends:
- freedesktop-sdk.bst:components/flatpak.bst
- filename:
  - freedesktop-sdk.bst:flatpak-images/mesa-extra.bst
  config:
    flatpak-image: true

config:
  arch: '%{gcc_arch}'
  branch: '%{sdk-branch-extra}'
  repo-mode: bare-user-only
