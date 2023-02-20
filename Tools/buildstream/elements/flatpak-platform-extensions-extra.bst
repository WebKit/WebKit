kind: flatpak_repo

build-depends:
- freedesktop-sdk.bst:components/flatpak.bst
- freedesktop-sdk.bst:flatpak-images/mesa-extra.bst

config:
  environment:
  - components/flatpak.bst
  arch: '%{gcc_arch}'
  branch: '%{sdk-branch-extra}'
  repo-mode: bare-user-only
