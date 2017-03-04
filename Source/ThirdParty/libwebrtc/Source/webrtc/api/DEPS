include_rules = [
  "+third_party/libyuv",
  "+webrtc/base",
  "+webrtc/common_video",
  "+webrtc/media",
  "+webrtc/p2p",
  "+webrtc/pc",
]

specific_include_rules = {
  "peerconnection_jni\.cc": [
    "+webrtc/voice_engine",
  ],

  # We allow .cc files in webrtc/api/ to #include a bunch of stuff
  # that's off-limits for the .h files. That's because .h files leak
  # their #includes to whoever's #including them, but .cc files do not
  # since no one #includes them.
  ".*\.cc": [
    "+webrtc/modules/audio_coding",
  ],
}
