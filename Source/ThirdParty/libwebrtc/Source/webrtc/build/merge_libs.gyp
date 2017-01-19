# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': ['common.gypi',],
  'variables': {
    'merge_libs_dependencies': [
    ],
  },
  'targets': [
    {
      'target_name': 'no_op',
      'type': 'executable',
      'dependencies': [
        '<@(merge_libs_dependencies)',
        '../webrtc.gyp:webrtc',
        '../p2p/p2p.gyp:rtc_p2p',
      ],
      'sources': ['no_op.cc',],
    },
    {
      'target_name': 'merged_lib',
      'type': 'none',
      'dependencies': [
        'no_op',
      ],
      'actions': [
        {
          'variables': {
            'output_lib_name': 'webrtc_merged',
            'output_lib': '<(PRODUCT_DIR)/<(STATIC_LIB_PREFIX)<(output_lib_name)<(STATIC_LIB_SUFFIX)',
          },
          'action_name': 'merge_libs',
          'inputs': ['<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)no_op<(EXECUTABLE_SUFFIX)'],
          'outputs': ['<(output_lib)'],
          'action': ['python',
                     'merge_libs.py',
                     '<(PRODUCT_DIR)',
                     '<(output_lib)',],
        },
      ],
    },
  ],
}
