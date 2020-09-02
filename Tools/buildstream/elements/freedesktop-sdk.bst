kind: junction

sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  # Changed to track git commit from upstream bst
  track: 'release/20.08'
  track-tags: true
  ref: freedesktop-sdk-20.08.0-0-g5931bbdc1a1d3da5f3218a718ff1b4f28d6b114f
- kind: patch
  path: patches/nettle-no-debug.diff
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
