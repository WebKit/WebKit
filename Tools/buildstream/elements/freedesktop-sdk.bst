kind: junction
sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  track: 'release/21.08'
  ref: freedesktop-sdk-21.08.9-25-gfb49cd02a3869721e08be44e068979ce98da6da6
- kind: patch
  path: patches/0001-binutils-import-upstream-patches-for-thin-archive-su.patch
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
