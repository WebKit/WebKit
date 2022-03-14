kind: junction
sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  track: 'release/21.08'
  ref: freedesktop-sdk-21.08.11-49-g655e58554588f1f7a41aeb6c88b8c7fdb0de7e6d
- kind: patch
  path: patches/fdo-0001-Bump-libnice-to-current-git-master-HEAD.patch
- kind: patch
  path: patches/fdo-0001-pango-Bump-to-1.50.4.patch
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
