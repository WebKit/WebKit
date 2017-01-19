# Copyright 2014 The LibYuv Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS. All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    'libyuv.gypi',
    '../../native_client/build/untrusted.gypi',
  ],
  'targets': [
    {
      'target_name': 'libyuv_nacl',
      'type': 'none',
      'variables': {
        'nlib_target': 'libyuv_nacl.a',
        'build_glibc': 0,
        'build_newlib': 0,
        'build_pnacl_newlib': 1,
      },
      'include_dirs': [
        'include',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
        ],
      },
      'sources': [
        '<@(libyuv_sources)',
      ],
    },  # target libyuv_nacl
  ]
}
