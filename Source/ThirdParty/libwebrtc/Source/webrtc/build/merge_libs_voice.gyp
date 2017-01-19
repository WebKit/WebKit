# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
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
      'target_name': 'no_op_voice',
      'type': 'executable',
      'dependencies': [
        '<@(merge_libs_dependencies)',
        '../voice_engine/voice_engine.gyp:voice_engine'
      ],
      'sources': ['no_op.cc',],
    },
    {
      'target_name': 'merged_lib_voice',
      'type': 'none',
      'dependencies': [
        'no_op_voice',
      ],
      'actions': [
        {
          'variables': {
            'output_lib_name': 'rtc_voice_merged',
            'output_lib': '<(PRODUCT_DIR)/<(STATIC_LIB_PREFIX)<(output_lib_name)<(STATIC_LIB_SUFFIX)',
          },
          'action_name': 'merge_libs_voice',
          'inputs': ['<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)no_op_voice<(EXECUTABLE_SUFFIX)'],
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
