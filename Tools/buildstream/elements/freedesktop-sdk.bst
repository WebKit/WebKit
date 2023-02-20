kind: junction
sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  track: 'release/22.08'
  ref: freedesktop-sdk-22.08.7-86-g200f2128224a8811b5e73341f8f3382c2d7fd464
- kind: patch
  path: patches/fdo-0001-pipewire-base-Disable-AEC-module.patch
- kind: patch
  path: patches/fdo-0001-libnice-Bump-to-0.1.21.patch
- kind: patch
  path: patches/fdo-0001-gst-plugins-bad-Add-support-Kate.patch
- kind: patch
  path: patches/fdo-0002-ffmpeg-Support-more-codecs.patch
- kind: patch
  path: patches/fdo-0003-gst-plugins-bad-Support-for-updated-webrtc-audio-pro.patch
- kind: patch
  path: patches/fdo-0004-gst-plugins-ugly-Enable-x264-encoder.patch
- kind: patch
  path: patches/fdo-0005-GStreamer-Bump-to-1.22.0.patch
- kind: patch
  path: patches/fdo-0006-gst-plugins-bad-Enable-soundtouch.patch
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
