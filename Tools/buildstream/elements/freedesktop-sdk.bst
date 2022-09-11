kind: junction
sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  track: 'release/22.08'
  ref: freedesktop-sdk-22.08.1-45-g0cc0140fac8eda36fa446cb8fa7c1a525fa56e7d
- kind: patch
  path: patches/fdo-0001-pipewire-base-Disable-AEC-module.patch
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
