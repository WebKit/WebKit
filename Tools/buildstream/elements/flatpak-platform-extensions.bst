kind: flatpak_repo

build-depends:
- freedesktop-sdk.bst:components/flatpak.bst

config:
  environment:
  - components/flatpak.bst
  arch: '%{gcc_arch}'
  branch: '1.0'
  repo-mode: bare-user-only
