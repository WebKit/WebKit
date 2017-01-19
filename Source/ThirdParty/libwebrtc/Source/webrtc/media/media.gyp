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
      'target_name': 'rtc_media',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/base/base.gyp:rtc_base_approved',
        '<(webrtc_root)/common.gyp:webrtc_common',
        '<(webrtc_root)/webrtc.gyp:webrtc',
        '<(webrtc_root)/voice_engine/voice_engine.gyp:voice_engine',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
        '<(webrtc_root)/p2p/p2p.gyp:rtc_p2p',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(libyuv_dir)/include',
        ],
      },
      'sources': [
        'base/adaptedvideotracksource.cc',
        'base/adaptedvideotracksource.h',
        'base/audiosource.h',
        'base/codec.cc',
        'base/codec.h',
        'base/cryptoparams.h',
        'base/device.h',
        'base/hybriddataengine.h',
        'base/mediachannel.h',
        'base/mediaconstants.cc',
        'base/mediaconstants.h',
        'base/mediaengine.cc',
        'base/mediaengine.h',
        'base/rtpdataengine.cc',
        'base/rtpdataengine.h',
        'base/rtpdump.cc',
        'base/rtpdump.h',
        'base/rtputils.cc',
        'base/rtputils.h',
        'base/streamparams.cc',
        'base/streamparams.h',
        'base/turnutils.cc',
        'base/turnutils.h',
        'base/videoadapter.cc',
        'base/videoadapter.h',
        'base/videobroadcaster.cc',
        'base/videobroadcaster.h',
        'base/videocapturer.cc',
        'base/videocapturer.h',
        'base/videocapturerfactory.h',
        'base/videocommon.cc',
        'base/videocommon.h',
        'base/videoframe.h',
        'base/videosourcebase.cc',
        'base/videosourcebase.h',
        'engine/nullwebrtcvideoengine.h',
        'engine/payload_type_mapper.cc',
        'engine/payload_type_mapper.h',
        'engine/simulcast.cc',
        'engine/simulcast.h',
        'engine/webrtccommon.h',
        'engine/webrtcmediaengine.cc',
        'engine/webrtcmediaengine.h',
        'engine/webrtcmediaengine.cc',
        'engine/webrtcvideocapturer.cc',
        'engine/webrtcvideocapturer.h',
        'engine/webrtcvideocapturerfactory.h',
        'engine/webrtcvideocapturerfactory.cc',
        'engine/webrtcvideodecoderfactory.h',
        'engine/webrtcvideoencoderfactory.cc',
        'engine/webrtcvideoencoderfactory.h',
        'engine/webrtcvideoengine2.cc',
        'engine/webrtcvideoengine2.h',
        'engine/webrtcvideoframe.h',
        'engine/webrtcvoe.h',
        'engine/webrtcvoiceengine.cc',
        'engine/webrtcvoiceengine.h',
        'sctp/sctpdataengine.cc',
        'sctp/sctpdataengine.h',
      ],
      # TODO(kjellander): Make the code compile without disabling these flags.
      # See https://bugs.chromium.org/p/webrtc/issues/detail?id=3307
      'cflags': [
        '-Wno-deprecated-declarations',
      ],
      'cflags!': [
        '-Wextra',
      ],
      'cflags_cc!': [
        '-Woverloaded-virtual',
      ],
      'msvs_disabled_warnings': [
        4245,  # conversion from 'int' to 'size_t', signed/unsigned mismatch.
        4267,  # conversion from 'size_t' to 'int', possible loss of data.
        4389,  # signed/unsigned mismatch.
      ],
      'conditions': [
        ['build_libyuv==1', {
          'dependencies': ['<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv',],
        }],
        ['build_usrsctp==1', {
          'include_dirs': [
            # TODO(jiayl): move this into the direct_dependent_settings of
            # usrsctp.gyp.
            '<(DEPTH)/third_party/usrsctp/usrsctplib',
          ],
          'dependencies': [
            '<(DEPTH)/third_party/usrsctp/usrsctp.gyp:usrsctplib',
          ],
        }],
        ['enable_intelligibility_enhancer==1', {
          'defines': ['WEBRTC_INTELLIGIBILITY_ENHANCER=1',],
        }, {
          'defines': ['WEBRTC_INTELLIGIBILITY_ENHANCER=0',],
        }],
        ['build_with_chromium==1', {
          'dependencies': [
            '<(webrtc_root)/modules/modules.gyp:video_capture',
          ],
        }, {
          'defines': [
            'HAVE_WEBRTC_VIDEO',
            'HAVE_WEBRTC_VOICE',
          ],
          'direct_dependent_settings': {
            'defines': [
              'HAVE_WEBRTC_VIDEO',
              'HAVE_WEBRTC_VOICE',
            ],
          },
          'dependencies': [
            '<(webrtc_root)/modules/modules.gyp:video_capture_module_internal_impl',
          ],
        }],
        ['OS=="linux" and use_gtk==1', {
          'sources': [
            'devices/gtkvideorenderer.cc',
            'devices/gtkvideorenderer.h',
          ],
          'cflags': [
            '<!@(pkg-config --cflags gobject-2.0 gthread-2.0 gtk+-2.0)',
          ],
        }],
      ],
    },  # target rtc_media
  ],  # targets.
}
