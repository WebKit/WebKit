kind: junction
sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  track: 'release/21.08'
  ref: freedesktop-sdk-21.08.2-22-gdc1e3345272fc70397b441832940b956fca5df14
- kind: patch
  path: patches/fdo-sdk-0001-Bump-ccache-to-4.4.2.patch
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
