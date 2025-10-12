kind: junction
sources:
- kind: git_repo
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  track: freedesktop-sdk-23.08*
  ref: freedesktop-sdk-23.08.6-0-gaa6e6977813fd567f6e0c56e6cd536e5e90d24eb
- kind: patch
  path: patches/fdo-0001-gtk3-Include-changes-from-downstream-WebKit-SDK.patch
- kind: patch
  path: patches/fdo-0003-libsoup-Vendor-patch-needed-for-test-bots.patch
- kind: patch
  path: patches/fdo-0004-libsoup-Vendor-another-libsoup-patch.patch
- kind: patch
  path: patches/fdo-0004-gst-plugins-ugly-Enable-x264-encoder.patch
- kind: patch
  path: patches/fdo-0005-GStreamer-Bump-to-1.26.4.patch
- kind: patch
  path: patches/fdo-0006-gst-plugins-bad-Enable-soundtouch.patch
- kind: patch
  path: patches/fdo-0007-Bump-to-Meson-1.7.0.patch
- kind: patch
  path: patches/fdo-0009-gst-plugins-bad-Enable-x265-encoder.patch
- kind: patch
  path: patches/fdo-0013-ffmpeg-Bump-to-version-7.0.2.patch
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
  overrides:
    components/dav1d.bst: sdk/dav1d.bst
    components/pango.bst: sdk/pango.bst
    components/noopenh264.bst: sdk/libopenh264.bst
    components/ccache.bst: sdk/ccache.bst
