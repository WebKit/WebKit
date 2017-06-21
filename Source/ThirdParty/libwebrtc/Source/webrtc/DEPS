# Define rules for which include paths are allowed in our source.
include_rules = [
  # Base is only used to build Android APK tests and may not be referenced by
  # WebRTC production code.
  "-base",
  "-chromium",
  "+external/webrtc/webrtc",  # Android platform build.
  "+gflags",
  "+libyuv",
  "-webrtc",  # Has to be disabled; otherwise all dirs below will be allowed.
  # Individual headers that will be moved out of here, see webrtc:4243.
  "+webrtc/common_types.h",
  "+webrtc/config.h",
  "+webrtc/transport.h",
  "+webrtc/typedefs.h",
  "+webrtc/video_receive_stream.h",
  "+webrtc/video_send_stream.h",
  "+webrtc/voice_engine_configurations.h",

  "+WebRTC",
  "+webrtc/api",
  "+webrtc/base",
  "+webrtc/modules/include",
  "+webrtc/test",
  "+webrtc/tools",
]

# The below rules will be removed when webrtc:4243 is fixed.
specific_include_rules = {
  "video_receive_stream\.h": [
    "+webrtc/common_video/include",
    "+webrtc/media/base",
  ],
  "video_send_stream\.h": [
    "+webrtc/common_video/include",
    "+webrtc/media/base",
  ],
}
