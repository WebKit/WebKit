kind: junction
sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  track: 'release/22.08'
  ref: freedesktop-sdk-22.08.13-17-g4cbd1d47a91cfa60269974bf84396dc12ff8df36
- kind: patch
  path: patches/fdo-0001-mesa-Bump-to-23.1.5.patch
- kind: patch
  path: patches/fdo-0001-pipewire-base-Disable-AEC-module.patch
- kind: patch
  path: patches/fdo-0001-gst-plugins-bad-Add-support-Kate.patch
- kind: patch
  path: patches/fdo-0002-ffmpeg-Support-more-codecs.patch
- kind: patch
  path: patches/fdo-0003-gst-plugins-bad-Support-for-updated-webrtc-audio-pro.patch
- kind: patch
  path: patches/fdo-0004-gst-plugins-ugly-Enable-x264-encoder.patch
- kind: patch
  path: patches/fdo-0005-GStreamer-Bump-to-1.22.6.patch
- kind: patch
  path: patches/fdo-0006-gst-plugins-bad-Enable-soundtouch.patch
- kind: patch
  path: patches/fdo-0007-components-Bump-GLib-to-version-2.76.patch
- kind: patch
  path: patches/fdo-0008-components-Bump-g-i-to-version-1.76.patch
- kind: patch
  path: patches/fdo-0009-gst-plugins-bad-Enable-x265-encoder.patch
- kind: patch
  path: patches/fdo-0010-gstreamer-Vendor-inter-patches-scheduled-for-1.22.7.patch
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
