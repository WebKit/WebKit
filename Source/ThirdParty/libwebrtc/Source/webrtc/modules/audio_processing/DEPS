include_rules = [
  "+audio/utility/audio_frame_operations.h",
  "+common_audio",
  "+system_wrappers",
]

specific_include_rules = {
  ".*test\.cc": [
    "+rtc_tools",
    # Android platform build has different paths.
    "+gtest",
    "+external/webrtc",
  ],
}
