kind: junction
sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  track: 'release/21.08'
  ref: freedesktop-sdk-21.08.11-68-gc5c9e4d4b64f099843657fabbab295b3b77403c6
- kind: patch
  path: patches/fdo-0001-Bump-libnice-to-current-git-master-HEAD.patch
- kind: patch
  path: patches/fdo-0001-pango-Bump-to-1.50.4.patch
- kind: patch
  path: patches/fdo-0001-meson-Bump-to-0.60.3.patch
- kind: patch
  path: patches/fdo-0001-shared-mime-info-Backport-meson-0.60-build-fix.patch
- kind: patch
  path: patches/fdo-0001-gdk-pixbuf-Remove-invalid-meson-options.patch
- kind: patch
  path: patches/fdo-0001-gst-plugins-bad-Remove-invalid-meson-option.patch
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
