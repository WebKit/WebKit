kind: junction

sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  # Changed to track git commit from upstream bst
  track: 'release/20.08'
  ref: freedesktop-sdk-20.08.4-26-g4421da17dc0a37cc6bb159eceb64b839841c7392
- kind: patch
  path: patches/nettle-no-debug.diff
- kind: patch
  path: patches/fdo-0001-pango-Bump-to-1.47.patch
- kind: patch
  path: patches/fdo-0001-meson-Bump-to-0.56.1.patch
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
