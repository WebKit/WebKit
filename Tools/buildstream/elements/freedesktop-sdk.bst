kind: junction
sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  track: 'release/21.08'
  ref: freedesktop-sdk-21.08.15-13-g376b04010f0971ab03d08802f3ad728f9044f130
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
  path: patches/fdo-0001-pipewire-base-Track-master-branch-and-disable-AEC-mo.patch
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
