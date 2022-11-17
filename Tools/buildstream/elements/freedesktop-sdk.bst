kind: junction
sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  track: 'release/22.08'
  ref: freedesktop-sdk-22.08.3-64-g754b0e0c6dc0e648a0f09fb5773612339555352f
- kind: patch
  path: patches/fdo-0001-pipewire-base-Disable-AEC-module.patch
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
