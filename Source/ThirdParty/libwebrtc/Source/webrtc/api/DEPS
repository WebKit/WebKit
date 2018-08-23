include_rules = [
  "+third_party/libyuv",
  "+common_video",
  "+media",
  "+p2p",
  "+pc",
  "+logging/rtc_event_log/rtc_event_log_factory_interface.h",
  "+modules/audio_processing/include",
  "+system_wrappers/include",
]

specific_include_rules = {
  # Needed because AudioEncoderOpus is in the wrong place for
  # backwards compatibilty reasons. See
  # https://bugs.chromium.org/p/webrtc/issues/detail?id=7847
  "audio_encoder_opus\.h": [
    "+modules/audio_coding/codecs/opus/audio_encoder_opus.h",
  ],

  # We allow .cc files in webrtc/api/ to #include a bunch of stuff
  # that's off-limits for the .h files. That's because .h files leak
  # their #includes to whoever's #including them, but .cc files do not
  # since no one #includes them.
  ".*\.cc": [
    "+modules/audio_coding",
    "+modules/audio_processing",
    "+modules/video_coding",
  ],
}
