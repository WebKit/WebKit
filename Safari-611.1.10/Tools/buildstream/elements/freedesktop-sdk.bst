kind: junction

sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  # Changed to track git commit from upstream bst
  track: 'release/20.08'
  ref: freedesktop-sdk-20.08.0-188-gd64042a0ddd5aeb0433a1c24a49da099c979a713
- kind: patch
  path: patches/nettle-no-debug.diff
- kind: patch
  path: patches/fdo-0001-pango-Bump-to-1.47.patch
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
