kind: junction
sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  track: 'release/21.08'
  ref: freedesktop-sdk-21.08.12-73-g6f02787ff2f4506731cf6aac153728e3251bdda1
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
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
