kind: junction
sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  track: 'release/22.08'
  ref: freedesktop-sdk-22.08.11-24-g67b3605b402cffed2db0344f1da3a6acb5b6c914
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
  path: patches/fdo-0005-GStreamer-Bump-to-1.22.2.patch
- kind: patch
  path: patches/fdo-0006-gst-plugins-bad-Enable-soundtouch.patch
config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
