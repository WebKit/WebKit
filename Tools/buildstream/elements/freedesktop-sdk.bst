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
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
