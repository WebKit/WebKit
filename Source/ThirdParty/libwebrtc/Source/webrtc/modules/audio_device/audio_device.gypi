# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'targets': [
    {
      'target_name': 'audio_device',
      'type': 'static_library',
      'dependencies': [
        'webrtc_utility',
        '<(webrtc_root)/base/base.gyp:rtc_base_approved',
        '<(webrtc_root)/common.gyp:webrtc_common',
        '<(webrtc_root)/common_audio/common_audio.gyp:common_audio',
        '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '.',
        '../include',
        'include',
        'dummy',  # Contains dummy audio device implementations.
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../include',
          'include',
        ],
      },
      # TODO(xians): Rename files to e.g. *_linux.{ext}, remove sources in conditions section
      'sources': [
        'include/audio_device.h',
        'include/audio_device_defines.h',
        'audio_device_buffer.cc',
        'audio_device_buffer.h',
        'audio_device_generic.cc',
        'audio_device_generic.h',
        'audio_device_config.h',
        'dummy/audio_device_dummy.cc',
        'dummy/audio_device_dummy.h',
        'dummy/file_audio_device.cc',
        'dummy/file_audio_device.h',
        'fine_audio_buffer.cc',
        'fine_audio_buffer.h',
      ],
      'conditions': [
        ['OS=="linux"', {
          'include_dirs': [
            'linux',
          ],
        }], # OS==linux
        ['OS=="ios"', {
          'include_dirs': [
            'ios',
          ],
        }], # OS==ios
        ['OS=="mac"', {
          'include_dirs': [
            'mac',
          ],
        }], # OS==mac
        ['OS=="win"', {
          'include_dirs': [
            'win',
          ],
        }],
        ['OS=="android"', {
          'include_dirs': [
            'android',
          ],
        }], # OS==android
        ['include_internal_audio_device==0', {
          'defines': [
            'WEBRTC_DUMMY_AUDIO_BUILD',
          ],
        }],
        ['build_with_chromium==0', {
          'sources': [
            # Don't link these into Chrome since they contain static data.
            'dummy/file_audio_device_factory.cc',
            'dummy/file_audio_device_factory.h',
          ],
        }],
        ['include_internal_audio_device==1', {
          'sources': [
            'audio_device_impl.cc',
            'audio_device_impl.h',
          ],
          'conditions': [
            ['use_dummy_audio_file_devices==1', {
              'defines': [
               'WEBRTC_DUMMY_FILE_DEVICES',
              ],
            }, { # use_dummy_audio_file_devices==0, so use a platform device.
              'conditions': [
                ['OS=="android"', {
                  'sources': [
                    'android/audio_device_template.h',
                    'android/audio_manager.cc',
                    'android/audio_manager.h',
                    'android/audio_record_jni.cc',
                    'android/audio_record_jni.h',
                    'android/audio_track_jni.cc',
                    'android/audio_track_jni.h',
                    'android/build_info.cc',
                    'android/build_info.h',
                    'android/opensles_common.cc',
                    'android/opensles_common.h',
                    'android/opensles_player.cc',
                    'android/opensles_player.h',
                    'android/opensles_recorder.cc',
                    'android/opensles_recorder.h',
                  ],
                  'link_settings': {
                    'libraries': [
                      '-llog',
                      '-lOpenSLES',
                    ],
                  },
                }],
                ['OS=="linux"', {
                  'sources': [
                    'linux/alsasymboltable_linux.cc',
                    'linux/alsasymboltable_linux.h',
                    'linux/audio_device_alsa_linux.cc',
                    'linux/audio_device_alsa_linux.h',
                    'linux/audio_mixer_manager_alsa_linux.cc',
                    'linux/audio_mixer_manager_alsa_linux.h',
                    'linux/latebindingsymboltable_linux.cc',
                    'linux/latebindingsymboltable_linux.h',
                  ],
                  'defines': [
                    'LINUX_ALSA',
                  ],
                  'link_settings': {
                    'libraries': [
                      '-ldl','-lX11',
                    ],
                  },
                  'conditions': [
                    ['include_pulse_audio==1', {
                      'defines': [
                        'LINUX_PULSE',
                      ],
                      'sources': [
                        'linux/audio_device_pulse_linux.cc',
                        'linux/audio_device_pulse_linux.h',
                        'linux/audio_mixer_manager_pulse_linux.cc',
                        'linux/audio_mixer_manager_pulse_linux.h',
                        'linux/pulseaudiosymboltable_linux.cc',
                        'linux/pulseaudiosymboltable_linux.h',
                      ],
                    }],
                  ],
                }],
                ['OS=="mac"', {
                  'sources': [
                    'mac/audio_device_mac.cc',
                    'mac/audio_device_mac.h',
                    'mac/audio_mixer_manager_mac.cc',
                    'mac/audio_mixer_manager_mac.h',
                    'mac/portaudio/pa_memorybarrier.h',
                    'mac/portaudio/pa_ringbuffer.c',
                    'mac/portaudio/pa_ringbuffer.h',
                  ],
                  'link_settings': {
                    'libraries': [
                      '$(SDKROOT)/System/Library/Frameworks/AudioToolbox.framework',
                      '$(SDKROOT)/System/Library/Frameworks/CoreAudio.framework',
                    ],
                  },
                }],
                ['OS=="ios"', {
                  'dependencies': [
                    '<(webrtc_root)/base/base.gyp:rtc_base',
                    '<(webrtc_root)/sdk/sdk.gyp:rtc_sdk_common_objc',
                  ],
                  'export_dependent_settings': [
                    '<(webrtc_root)/sdk/sdk.gyp:rtc_sdk_common_objc',
                  ],
                  'sources': [
                    'ios/audio_device_ios.h',
                    'ios/audio_device_ios.mm',
                    'ios/audio_device_not_implemented_ios.mm',
                    'ios/audio_session_observer.h',
                    'ios/objc/RTCAudioSession+Configuration.mm',
                    'ios/objc/RTCAudioSession+Private.h',
                    'ios/objc/RTCAudioSession.h',
                    'ios/objc/RTCAudioSession.mm',
                    'ios/objc/RTCAudioSessionConfiguration.h',
                    'ios/objc/RTCAudioSessionConfiguration.m',
                    'ios/objc/RTCAudioSessionDelegateAdapter.h',
                    'ios/objc/RTCAudioSessionDelegateAdapter.mm',
                    'ios/voice_processing_audio_unit.h',
                    'ios/voice_processing_audio_unit.mm',
                  ],
                  'xcode_settings': {
                    'CLANG_ENABLE_OBJC_ARC': 'YES',
                  },
                  'link_settings': {
                    'xcode_settings': {
                      'OTHER_LDFLAGS': [
                        '-framework AudioToolbox',
                        '-framework AVFoundation',
                        '-framework Foundation',
                        '-framework UIKit',
                      ],
                    },
                  },
                }],
                ['OS=="win"', {
                  'sources': [
                    'win/audio_device_core_win.cc',
                    'win/audio_device_core_win.h',
                    'win/audio_device_wave_win.cc',
                    'win/audio_device_wave_win.h',
                    'win/audio_mixer_manager_win.cc',
                    'win/audio_mixer_manager_win.h',
                  ],
                  'link_settings': {
                    'libraries': [
                      # Required for the built-in WASAPI AEC.
                      '-ldmoguids.lib',
                      '-lwmcodecdspuuid.lib',
                      '-lamstrmid.lib',
                      '-lmsdmo.lib',
                    ],
                  },
                }],
                ['OS=="win" and clang==1', {
                  'msvs_settings': {
                    'VCCLCompilerTool': {
                      'AdditionalOptions': [
                        # Disable warnings failing when compiling with Clang on Windows.
                        # https://bugs.chromium.org/p/webrtc/issues/detail?id=5366
                        '-Wno-bool-conversion',
                        '-Wno-delete-non-virtual-dtor',
                        '-Wno-logical-op-parentheses',
                        '-Wno-microsoft-extra-qualification',
                        '-Wno-microsoft-goto',
                        '-Wno-missing-braces',
                        '-Wno-parentheses-equality',
                        '-Wno-reorder',
                        '-Wno-shift-overflow',
                        '-Wno-tautological-compare',
                        '-Wno-unused-private-field',
                      ],
                    },
                  },
                }],
              ], # conditions (for non-dummy devices)
            }], # use_dummy_audio_file_devices check
          ], # conditions
        }], # include_internal_audio_device==1
      ], # conditions
    },
  ],
}

