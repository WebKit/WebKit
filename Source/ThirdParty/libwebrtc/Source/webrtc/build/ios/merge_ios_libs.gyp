# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': ['../common.gypi',],
  'conditions': [
    ['OS=="ios" or OS=="mac"', {
      'targets': [
        {
          'target_name': 'rtc_sdk_peerconnection_objc_no_op',
          'includes': [ 'objc_app.gypi' ],
          'type': 'executable',
          'dependencies': [
            '<(webrtc_root)/sdk/sdk.gyp:rtc_sdk_peerconnection_objc',
          ],
          'sources': ['no_op.cc',],
        },
      ],
    }]
  ],
}
