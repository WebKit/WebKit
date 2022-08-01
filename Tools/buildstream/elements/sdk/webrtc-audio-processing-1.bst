kind: meson

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- sdk/abseil-cpp.bst

variables:
  meson-local: >-
    -Dcpp_std=c++17

public:
  bst:
    split-rules:
      devel:
        (>):
        - "%{libdir}/libwebrtc_audio_processing.so"
        - "%{libdir}/libwebrtc_audio_coding.so"

sources:
- kind: git_tag
  url: gitlab_freedesktop_org:pulseaudio/webrtc-audio-processing.git
  track: master
  ref: v1.0-22-gca1186946d3c9d08229c02cc7a46c3caf766ca2e
