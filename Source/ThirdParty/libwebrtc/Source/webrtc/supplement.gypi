{
  'variables': {
    'variables': {
      'webrtc_root%': '<(DEPTH)/webrtc',
      # Override the defaults in Chromium's build/common.gypi.
      # Needed for ARC and libc++.
      'mac_sdk_min%': '10.11',
      'mac_deployment_target%': '10.7',
    },
    'webrtc_root%': '<(webrtc_root)',
    'mac_deployment_target%': '<(mac_deployment_target)',
    'use_sysroot%': '<(use_sysroot)',
    'build_with_chromium': 0,
    'conditions': [
      ['OS=="ios"', {
        # Set target_subarch for if not already set. This is needed because the
        # Chromium iOS toolchain relies on target_subarch being set.
        'conditions': [
          ['target_arch=="arm" or target_arch=="ia32"', {
            'target_subarch%': 'arm32',
          }],
          ['target_arch=="arm64" or target_arch=="x64"', {
            'target_subarch%': 'arm64',
          }],
        ],
      }],
      ['OS=="android"', {
        # MJPEG capture is not used on Android. Disable to reduce
        # libjingle_peerconnection_so file size.
        'libyuv_disable_jpeg%': 1,
      }],
    ],
  },
  'target_defaults': {
    'target_conditions': [
      ['_target_name=="sanitizer_options"', {
        'conditions': [
          ['lsan==1', {
            # Replace Chromium's LSan suppressions with our own for WebRTC.
            'sources/': [
              ['exclude', 'lsan_suppressions.cc'],
            ],
            'sources': [
              '<(webrtc_root)/build/sanitizers/lsan_suppressions_webrtc.cc',
            ],
          }],
          ['tsan==1', {
            # Replace Chromium's TSan v2 suppressions with our own for WebRTC.
            'sources/': [
              ['exclude', 'tsan_suppressions.cc'],
            ],
            'sources': [
              '<(webrtc_root)/build/sanitizers/tsan_suppressions_webrtc.cc',
            ],
          }],
        ],
      }],
    ],
  },
}
