kind: junction
sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  track: 'release/22.08'
  ref: freedesktop-sdk-22.08.5-56-gaa9d9e526a5d5da18a4bdaa64d5bd4c4de1f8863
- kind: patch
  path: patches/fdo-0001-pipewire-base-Disable-AEC-module.patch
- kind: patch
  path: patches/fdo-0001-libnice-Bump-to-0.1.21.patch
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
