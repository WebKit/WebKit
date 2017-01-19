 # Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This file is used for internal tools used by the WebRTC code only.

{
  'includes': [
    '../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'command_line_parser',
      'type': 'static_library',
      'sources': [
        'simple_command_line_parser.h',
        'simple_command_line_parser.cc',
      ],
      'dependencies': [
        '<(webrtc_root)/base/base.gyp:gtest_prod',
      ],
    }, # command_line_parser
  ],
}
