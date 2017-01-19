# Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'variables': {
    'include_examples%': 1,
    'include_tests%': 1,
  },
  'includes': [
    'webrtc/build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        'webrtc/api/api.gyp:*',
        'webrtc/base/base.gyp:*',
        'webrtc/common.gyp:*',
        'webrtc/common_audio/common_audio.gyp:*',
        'webrtc/common_video/common_video.gyp:*',
        'webrtc/media/media.gyp:*',
        'webrtc/modules/modules.gyp:*',
        'webrtc/p2p/p2p.gyp:*',
        'webrtc/pc/pc.gyp:*',
        'webrtc/stats/stats.gyp:*',
        'webrtc/system_wrappers/system_wrappers.gyp:*',
        'webrtc/tools/tools.gyp:*',
        'webrtc/voice_engine/voice_engine.gyp:*',
        'webrtc/webrtc.gyp:*',
        '<(webrtc_vp8_dir)/vp8.gyp:*',
        '<(webrtc_vp9_dir)/vp9.gyp:*',
      ],
      'conditions': [
        ['OS=="android" and build_with_chromium==0', {
          'dependencies': [
            # No longer supported, please refer to GN targets.
            #'webrtc/api/api_java.gyp:*',
          ],
        }],
        ['include_tests==1', {
          'includes': [
            'webrtc/webrtc_tests.gypi',
          ],
          'dependencies': [
            'webrtc/test/test.gyp:*',
          ],
        }],
        ['include_examples==1', {
          'dependencies': [
            'webrtc/webrtc_examples.gyp:*',
          ],
        }],
        ['(OS=="ios" or (OS=="mac" and mac_deployment_target=="10.7"))', {
          'dependencies': [
            'webrtc/sdk/sdk.gyp:*',
          ],
        }],
      ],
    },
  ],
}
