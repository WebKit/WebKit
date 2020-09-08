kind: junction

sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  # Changed to track git commit from upstream bst
  track: 'release/20.08'
  ref: freedesktop-sdk-20.08.0-43-gfb14cd9cb1ba18839dd22f4ab41484eef9ab87ae
- kind: patch
  path: patches/nettle-no-debug.diff
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
