kind: junction

sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  # Changed to track git commit from upstream bst
  track: 'release/20.08'
  ref: freedesktop-sdk-20.08.13-29-g096698709baa9286c4d8a1143a1b094eca3ff844
- kind: patch
  path: patches/nettle-no-debug.diff
- kind: patch
  path: patches/fdo-0001-pango-Bump-to-1.47.patch
- kind: patch
  path: patches/fdo-0001-dav1d-Bump-to-version-0.8.2.patch
- kind: patch
  path: patches/fdo-0001-libvpx-Enable-high-bit-depth-support.patch
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
