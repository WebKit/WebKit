kind: junction

sources:
- kind: git_tag
  url: gitlab_com:freedesktop-sdk/freedesktop-sdk.git
  # Changed to track git commit from upstream bst
  track: '19.08'
  track-tags: true
  ref: freedesktop-sdk-19.08.10-0-gf24d6f4dd60da0c94c61a3993f5992fa28219a8f
- kind: patch
  path: patches/nettle-no-debug.diff
- kind: patch
  path: patches/fdo-0001-Update-gcc-to-9.3.0.patch
- kind: patch
  path: patches/fdo/0001-components-xdg-desktop-portal.bst-Update-to-1.6.0.patch
- kind: patch
  path: patches/fdo/0001-Add-Jack.patch
- kind: patch
  path: patches/fdo/0002-Update-PipeWire-to-0.3.1.patch
- kind: patch
  path: patches/fdo/0001-ffmpeg-Add-MS-MPEG-DivX-variants.patch
- kind: patch
  path: patches/fdo/0002-ffmpeg-Add-VP6-and-Sorenson-Spark-video-codecs.patch
- kind: patch
  path: patches/fdo/0003-ffmpeg-Add-Intel-Indeo-and-Cinepak-video-support.patch
- kind: patch
  path: patches/fdo/0004-ffmpeg-Add-MPEG-2-video-decoder.patch
- kind: patch
  path: patches/fdo/0005-ffmpeg-Add-msmpeg4v3-support.patch

config:
  options:
    target_arch: '%{arch}'
    bootstrap_build_arch: '%{bootstrap_build_arch}'
