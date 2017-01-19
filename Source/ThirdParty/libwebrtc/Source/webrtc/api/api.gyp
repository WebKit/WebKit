# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [ '../build/common.gypi', ],
  'conditions': [
    ['os_posix == 1 and OS != "mac" and OS != "ios"', {
      'conditions': [
        ['sysroot!=""', {
          'variables': {
            'pkg-config': '../../../build/linux/pkg-config-wrapper "<(sysroot)" "<(target_arch)"',
          },
        }, {
          'variables': {
            'pkg-config': 'pkg-config'
          },
        }],
      ],
    }],
    # Excluded from the Chromium build since they cannot be built due to
    # incompability with Chromium's logging implementation.
    ['OS=="android" and build_with_chromium==0', {
      'targets': [
        {
          'target_name': 'libjingle_peerconnection_jni',
          'type': 'static_library',
          'dependencies': [
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:field_trial_default',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:metrics_default',
            'libjingle_peerconnection',
          ],
          'sources': [
            'android/jni/androidmediacodeccommon.h',
            'android/jni/androidmediadecoder_jni.cc',
            'android/jni/androidmediadecoder_jni.h',
            'android/jni/androidmediaencoder_jni.cc',
            'android/jni/androidmediaencoder_jni.h',
            'android/jni/androidmetrics_jni.cc',
            'android/jni/androidnetworkmonitor_jni.cc',
            'android/jni/androidnetworkmonitor_jni.h',
            'android/jni/androidvideotracksource_jni.cc',
            'android/jni/classreferenceholder.cc',
            'android/jni/classreferenceholder.h',
            'android/jni/jni_helpers.cc',
            'android/jni/jni_helpers.h',
            'android/jni/native_handle_impl.cc',
            'android/jni/native_handle_impl.h',
            'android/jni/peerconnection_jni.cc',
            'android/jni/surfacetexturehelper_jni.cc',
            'android/jni/surfacetexturehelper_jni.h',
            'androidvideotracksource.cc',
            'androidvideotracksource.h',
          ],
          'include_dirs': [
            '<(libyuv_dir)/include',
          ],
          # TODO(kjellander): Make the code compile without disabling these flags.
          # See https://bugs.chromium.org/p/webrtc/issues/detail?id=3307
          'cflags': [
            '-Wno-sign-compare',
            '-Wno-unused-variable',
          ],
          'cflags!': [
            '-Wextra',
          ],
          'msvs_disabled_warnings': [
            4245,  # conversion from 'int' to 'size_t', signed/unsigned mismatch.
            4267,  # conversion from 'size_t' to 'int', possible loss of data.
            4389,  # signed/unsigned mismatch.
          ],
        },
        {
          'target_name': 'libjingle_peerconnection_so',
          'type': 'shared_library',
          'dependencies': [
            'libjingle_peerconnection',
            'libjingle_peerconnection_jni',
          ],
          'sources': [
           'android/jni/jni_onload.cc',
          ],
          'variables': {
            # This library uses native JNI exports; tell GYP so that the
            # required symbols will be kept.
            'use_native_jni_exports': 1,
          },
        },
      ]
    }],
  ],  # conditions
  'targets': [
    {
      'target_name': 'call_api',
      'type': 'static_library',
      'dependencies': [
        # TODO(kjellander): Add remaining dependencies when webrtc:4243 is done.
        '<(webrtc_root)/base/base.gyp:rtc_base_approved',
        '<(webrtc_root)/common.gyp:webrtc_common',
        '<(webrtc_root)/modules/modules.gyp:audio_encoder_interface',
      ],
      'sources': [
        'call/audio_receive_stream.h',
        'call/audio_send_stream.cc',
        'call/audio_send_stream.h',
        'call/audio_sink.h',
        'call/audio_state.h',
        'call/flexfec_receive_stream.h'
      ],
    },
    {
      'target_name': 'libjingle_peerconnection',
      'type': 'static_library',
      'dependencies': [
        ':call_api',
        ':rtc_stats_api',
        '<(webrtc_root)/media/media.gyp:rtc_media',
        '<(webrtc_root)/pc/pc.gyp:rtc_pc',
        '<(webrtc_root)/stats/stats.gyp:rtc_stats',
      ],
      'sources': [
        'audiotrack.cc',
        'audiotrack.h',
        'datachannel.cc',
        'datachannel.h',
        'datachannelinterface.h',
        'dtmfsender.cc',
        'dtmfsender.h',
        'dtmfsenderinterface.h',
        'jsep.h',
        'jsepicecandidate.cc',
        'jsepicecandidate.h',
        'jsepsessiondescription.cc',
        'jsepsessiondescription.h',
        'localaudiosource.cc',
        'localaudiosource.h',
        'mediaconstraintsinterface.cc',
        'mediaconstraintsinterface.h',
        'mediacontroller.cc',
        'mediacontroller.h',
        'mediastream.cc',
        'mediastream.h',
        'mediastreaminterface.h',
        'mediastreamobserver.cc',
        'mediastreamobserver.h',
        'mediastreamproxy.h',
        'mediastreamtrack.h',
        'mediastreamtrackproxy.h',
        'notifier.h',
        'peerconnection.cc',
        'peerconnection.h',
        'peerconnectionfactory.cc',
        'peerconnectionfactory.h',
        'peerconnectionfactoryproxy.h',
        'peerconnectioninterface.h',
        'peerconnectionproxy.h',
        'proxy.h',
        'remoteaudiosource.cc',
        'remoteaudiosource.h',
        'rtcstatscollector.cc',
        'rtcstatscollector.h',
        'rtpparameters.h',
        'rtpreceiver.cc',
        'rtpreceiver.h',
        'rtpreceiverinterface.h',
        'rtpsender.cc',
        'rtpsender.h',
        'rtpsenderinterface.h',
        'sctputils.cc',
        'sctputils.h',
        'statscollector.cc',
        'statscollector.h',
        'statstypes.cc',
        'statstypes.h',
        'streamcollection.h',
        'videocapturertracksource.cc',
        'videocapturertracksource.h',
        'videosourceproxy.h',
        'videotrack.cc',
        'videotrack.h',
        'videotracksource.cc',
        'videotracksource.h',
        'webrtcsdp.cc',
        'webrtcsdp.h',
        'webrtcsession.cc',
        'webrtcsession.h',
        'webrtcsessiondescriptionfactory.cc',
        'webrtcsessiondescriptionfactory.h',
      ],
      'conditions': [
        ['clang==1', {
          'cflags!': [
            '-Wextra',
          ],
          'xcode_settings': {
            'WARNING_CFLAGS!': ['-Wextra'],
          },
        }, {
          'cflags': [
            '-Wno-maybe-uninitialized',  # Only exists for GCC.
          ],
        }],
        ['use_quic==1', {
          'dependencies': [
            '<(DEPTH)/third_party/libquic/libquic.gyp:libquic',
          ],
          'sources': [
            'quicdatachannel.cc',
            'quicdatachannel.h',
            'quicdatatransport.cc',
            'quicdatatransport.h',
          ],
          'export_dependent_settings': [
            '<(DEPTH)/third_party/libquic/libquic.gyp:libquic',
          ],
        }],
      ],
    },  # target libjingle_peerconnection
    {
      # GN version: webrtc/api:rtc_stats_api
      'target_name': 'rtc_stats_api',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/base/base.gyp:rtc_base_approved',
      ],
      'sources': [
        'stats/rtcstats.h',
        'stats/rtcstats_objects.h',
        'stats/rtcstatsreport.h',
      ],
    },  # target rtc_stats_api
    {
      # GN version: webrtc/api:audio_mixer_api
      'target_name': 'audio_mixer_api',
      'type': 'static_library',
      'dependencies': [
        '<(webrtc_root)/base/base.gyp:rtc_base_approved',
      ],
      'sources': [
        'audio/audio_mixer.h',
      ],
    },  # target rtc_stats_api
  ],  # targets
}
