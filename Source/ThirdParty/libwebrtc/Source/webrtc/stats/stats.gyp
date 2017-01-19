# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [ '../build/common.gypi', ],
  'targets': [
    {
      # GN version: webrtc/stats:rtc_stats
      'target_name': 'rtc_stats',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/api/api.gyp:rtc_stats_api',
        '<(webrtc_root)/base/base.gyp:rtc_base_approved',
      ],
      'sources': [
        'rtcstats.cc',
        'rtcstats_objects.cc',
        'rtcstatsreport.cc',
      ],
    },
  ],
}
