kind: junction

sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  # Changed to track git commit from upstream bst
  track: '19.08'
  track-tags: true
  ref: freedesktop-sdk-19.08.11-0-gac942905b94dfeda1b2d0d45d8ca3f8110e0ebe3
- kind: patch
  path: patches/nettle-no-debug.diff
- kind: patch
  path: patches/fdo-0001-Update-gcc-to-9.3.0.patch
- kind: patch
  path: patches/fdo/0001-Pango-Bump-to-1.45-master.patch
- kind: patch
  path: patches/fdo/0001-dav1d-Bump-to-0.7.0.patch
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
