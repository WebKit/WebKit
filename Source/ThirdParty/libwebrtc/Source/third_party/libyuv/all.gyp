# Copyright 2013 The LibYuv Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS. All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# all.gyp and All target are for benefit of android gyp build.
{
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        'libyuv.gyp:*',
        'libyuv_test.gyp:*',
      ],
    },
  ],
}
