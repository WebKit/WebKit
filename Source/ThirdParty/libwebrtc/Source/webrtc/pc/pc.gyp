# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': ['../build/common.gypi'],
  'variables': {
    'rtc_pc_defines': [
      'HAVE_SCTP',
      'HAVE_SRTP',
    ],
  },
  'targets': [
    {
      'target_name': 'rtc_pc',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/base/base.gyp:rtc_base_approved',
        '<(webrtc_root)/media/media.gyp:rtc_media',
      ],
      'conditions': [
        ['build_with_chromium==1', {
          'sources': [
            'externalhmac.h',
            'externalhmac.cc',
          ],
        }],
        ['build_libsrtp==1', {
          'dependencies': [
            '<(DEPTH)/third_party/libsrtp/libsrtp.gyp:libsrtp',
          ],
        }],
      ],
      'defines': [
        '<@(rtc_pc_defines)',
      ],
      'include_dirs': [
        '<(DEPTH)/testing/gtest/include',
      ],
      'direct_dependent_settings': {
        'defines': [
          '<@(rtc_pc_defines)'
        ],
        'include_dirs': [
          '<(DEPTH)/testing/gtest/include',
        ],
      },
      'sources': [
        'audiomonitor.cc',
        'audiomonitor.h',
        'bundlefilter.cc',
        'bundlefilter.h',
        'channel.cc',
        'channel.h',
        'channelmanager.cc',
        'channelmanager.h',
        'currentspeakermonitor.cc',
        'currentspeakermonitor.h',
        'mediamonitor.cc',
        'mediamonitor.h',
        'mediasession.cc',
        'mediasession.h',
        'rtcpmuxfilter.cc',
        'rtcpmuxfilter.h',
        'srtpfilter.cc',
        'srtpfilter.h',
        'voicechannel.h',
      ],
    },  # target rtc_pc
  ],  # targets
}
