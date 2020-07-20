kind: junction

sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  # Changed to track git commit from upstream bst
  track: '19.08'
  track-tags: true
  ref: freedesktop-sdk-19.08.12-0-ga7778d9c98b5fbff712c2c8b17e3e804df9baaab
- kind: patch
  path: patches/nettle-no-debug.diff
- kind: patch
  path: patches/fdo/0001-Pango-Bump-to-1.45-master.patch
- kind: patch
  path: patches/fdo/0001-dav1d-Bump-to-0.7.0.patch
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
