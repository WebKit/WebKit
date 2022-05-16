kind: junction
sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  track: 'release/21.08'
  ref: freedesktop-sdk-21.08.13-31-g21fed0ebe165b748f4bbf1edd0f275dbabc26086
- kind: patch
  path: patches/fdo-0001-Bump-libnice-to-current-git-master-HEAD.patch
- kind: patch
  path: patches/fdo-0001-pango-Bump-to-1.50.4.patch
- kind: patch
  path: patches/fdo-0001-gdk-pixbuf-Remove-invalid-meson-options.patch
- kind: patch
  path: patches/fdo-0001-gst-plugins-bad-Remove-invalid-meson-option.patch
- kind: patch
  path: patches/fdo-0001-gobject-introspection-Bump-to-1.72.patch
- kind: patch
  path: patches/fdo-0002-meson-Bump-to-1.62.patch
- kind: patch
  path: patches/fdo-0001-pipewire-base-Track-master-branch.patch
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
