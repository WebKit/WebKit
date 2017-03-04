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
  "+webrtc/call.h",
  "+webrtc/common_types.h",
  "+webrtc/config.h",
  "+webrtc/transport.h",
  "+webrtc/typedefs.h",
  "+webrtc/video_decoder.h",
  "+webrtc/video_encoder.h",
  "+webrtc/video_frame.h",
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
  # The call/call.h exception is here only until the peerconnection
  # implementation has been moved out of api/. See:
  # http://bugs.webrtc.org/5883
  "call\.h": [
    "+webrtc/call/call.h"
  ],
  "video_frame\.h": [
    "+webrtc/common_video",
  ],
  "video_receive_stream\.h": [
    "+webrtc/common_video/include",
    "+webrtc/media/base",
  ],
  "video_send_stream\.h": [
    "+webrtc/common_video/include",
    "+webrtc/media/base",
  ],
}
