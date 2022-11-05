kind: junction
sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  track: 'release/22.08'
  ref: freedesktop-sdk-22.08.2.1-168-g9b732aab03152bc450e206c630d9dfd187c318c3
- kind: patch
  path: patches/fdo-0001-pipewire-base-Disable-AEC-module.patch
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
