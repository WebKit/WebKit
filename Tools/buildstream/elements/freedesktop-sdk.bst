kind: junction
sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  track: 'release/22.08'
  ref: freedesktop-sdk-22.08.2.1-65-gfc506a76fb04b35566582a25203828aa2fb5e281
- kind: patch
  path: patches/fdo-0001-pipewire-base-Disable-AEC-module.patch
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
