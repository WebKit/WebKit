# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This file contains common settings for building WebRTC components.

{
  # Nesting is required in order to use variables for setting other variables.
  'variables': {
    'variables': {
      'variables': {
        'variables': {
          # This will already be set to zero by supplement.gypi
          'build_with_chromium%': 1,

          # Enable to use the Mozilla internal settings.
          'build_with_mozilla%': 0,
        },
        'build_with_chromium%': '<(build_with_chromium)',
        'build_with_mozilla%': '<(build_with_mozilla%)',
        'include_opus%': 1,

        'conditions': [
          # Include the iLBC audio codec?
          ['build_with_chromium==1 or build_with_mozilla==1', {
            'include_ilbc%': 0,
          }, {
            'include_ilbc%': 1,
          }],

          ['build_with_chromium==1', {
            'webrtc_root%': '<(DEPTH)/third_party/webrtc',
          }, {
            'webrtc_root%': '<(DEPTH)/webrtc',
          }],

          # Controls whether we use libevent on posix platforms.
          # TODO(phoglund): should arguably be controlled by platform #ifdefs
          # in the code instead.
          ['OS=="win" or OS=="mac" or OS=="ios"', {
            'build_libevent%': 0,
            'enable_libevent%': 0,
          }, {
            'build_libevent%': 1,
            'enable_libevent%': 1,
          }],
        ],
      },
      'build_with_chromium%': '<(build_with_chromium)',
      'build_with_mozilla%': '<(build_with_mozilla)',
      'build_libevent%': '<(build_libevent)',
      'enable_libevent%': '<(enable_libevent)',
      'webrtc_root%': '<(webrtc_root)',
      'webrtc_vp8_dir%': '<(webrtc_root)/modules/video_coding/codecs/vp8',
      'webrtc_vp9_dir%': '<(webrtc_root)/modules/video_coding/codecs/vp9',
      'include_ilbc%': '<(include_ilbc)',
      'include_opus%': '<(include_opus)',
      'opus_dir%': '<(DEPTH)/third_party/opus',
    },
    'build_with_chromium%': '<(build_with_chromium)',
    'build_with_mozilla%': '<(build_with_mozilla)',
    'build_libevent%': '<(build_libevent)',
    'enable_libevent%': '<(enable_libevent)',
    'webrtc_root%': '<(webrtc_root)',
    'test_runner_path': '<(DEPTH)/webrtc/build/android/test_runner.py',
    'webrtc_vp8_dir%': '<(webrtc_vp8_dir)',
    'webrtc_vp9_dir%': '<(webrtc_vp9_dir)',
    'include_ilbc%': '<(include_ilbc)',
    'include_opus%': '<(include_opus)',
    'rtc_relative_path%': 1,
    'external_libraries%': '0',
    'json_root%': '<(DEPTH)/third_party/jsoncpp/source/include/',
    # openssl needs to be defined or gyp will complain. Is is only used when
    # when providing external libraries so just use current directory as a
    # placeholder.
    'ssl_root%': '.',

    # The Chromium common.gypi we use treats all gyp files without
    # chromium_code==1 as third party code. This disables many of the
    # preferred warning settings.
    #
    # We can set this here to have WebRTC code treated as Chromium code. Our
    # third party code will still have the reduced warning settings.
    'chromium_code': 1,

    # Targets are by default not NaCl untrusted code. Use this variable exclude
    # code that uses libraries that aren't available in the NaCl sandbox.
    'nacl_untrusted_build%': 0,

    # Set to 1 to enable code coverage on Linux using the gcov library.
    'coverage%': 0,

    # Set to "func", "block", "edge" for coverage generation.
    # At unit test runtime set UBSAN_OPTIONS="coverage=1".
    # It is recommend to set include_examples=0.
    # Use llvm's sancov -html-report for human readable reports.
    # See http://clang.llvm.org/docs/SanitizerCoverage.html .
    'webrtc_sanitize_coverage%': "",

    # Remote bitrate estimator logging/plotting.
    'enable_bwe_test_logging%': 0,

    # Selects fixed-point code where possible.
    'prefer_fixed_point%': 0,

    # Enables the use of protocol buffers for debug recordings.
    'enable_protobuf%': 1,

    # Disable the code for the intelligibility enhancer by default.
    'enable_intelligibility_enhancer%': 0,

    # Selects whether debug dumps for the audio processing module
    # should be generated.
    'apm_debug_dump%': 0,

    # Disable these to not build components which can be externally provided.
    'build_expat%': 1,
    'build_json%': 1,
    'build_libsrtp%': 1,
    'build_libvpx%': 1,
    'libvpx_build_vp9%': 1,
    'build_libyuv%': 1,
    'build_openmax_dl%': 1,
    'build_opus%': 1,
    'build_protobuf%': 1,
    'build_ssl%': 1,
    'build_usrsctp%': 1,

    # Disable by default
    'have_dbus_glib%': 0,

    # Make it possible to provide custom locations for some libraries.
    'libvpx_dir%': '<(DEPTH)/third_party/libvpx',
    'libyuv_dir%': '<(DEPTH)/third_party/libyuv',
    'opus_dir%': '<(opus_dir)',

    # Use Java based audio layer as default for Android.
    # Change this setting to 1 to use Open SL audio instead.
    # TODO(henrika): add support for Open SL ES.
    'enable_android_opensl%': 0,

    # Link-Time Optimizations
    # Executes code generation at link-time instead of compile-time
    # https://gcc.gnu.org/wiki/LinkTimeOptimization
    'use_lto%': 0,

    # Defer ssl perference to that specified through sslconfig.h instead of
    # choosing openssl or nss directly.  In practice, this can be used to
    # enable schannel on windows.
    'use_legacy_ssl_defaults%': 0,

    # Determines whether NEON code will be built.
    'build_with_neon%': 0,

    # Disable this to skip building source requiring GTK.
    'use_gtk%': 1,

    # Enable this to prevent extern symbols from being hidden on iOS builds.
    # The chromium settings we inherit hide symbols by default on Release
    # builds. We want our symbols to be visible when distributing WebRTC via
    # static libraries to avoid linker warnings.
    'ios_override_visibility%': 0,

    # Determines whether QUIC code will be built.
    'use_quic%': 0,

    # By default, use normal platform audio support or dummy audio, but don't
    # use file-based audio playout and record.
    'use_dummy_audio_file_devices%': 0,

    'conditions': [
      # Enable this to build OpenH264 encoder/FFmpeg decoder. This is supported
      # on all platforms except Android and iOS. Because FFmpeg can be built
      # with/without H.264 support, |ffmpeg_branding| has to separately be set
      # to a value that includes H.264, for example "Chrome". If FFmpeg is built
      # without H.264, compilation succeeds but |H264DecoderImpl| fails to
      # initialize. See also: |rtc_initialize_ffmpeg|.
      # CHECK THE OPENH264, FFMPEG AND H.264 LICENSES/PATENTS BEFORE BUILDING.
      # http://www.openh264.org, https://www.ffmpeg.org/
      ['proprietary_codecs==1 and OS!="android" and OS!="ios"', {
        'rtc_use_h264%': 1,
      }, {
        'rtc_use_h264%': 0,
      }],

      # FFmpeg must be initialized for |H264DecoderImpl| to work. This can be
      # done by WebRTC during |H264DecoderImpl::InitDecode| or externally.
      # FFmpeg must only be initialized once. Projects that initialize FFmpeg
      # externally, such as Chromium, must turn this flag off so that WebRTC
      # does not also initialize.
      ['build_with_chromium==0', {
        'rtc_initialize_ffmpeg%': 1,
      }, {
        'rtc_initialize_ffmpeg%': 0,
      }],

      ['build_with_chromium==1', {
        # Build sources requiring GTK. NOTICE: This is not present in Chrome OS
        # build environments, even if available for Chromium builds.
        'use_gtk%': 0,
        # Exclude pulse audio on Chromium since its prerequisites don't require
        # pulse audio.
        'include_pulse_audio%': 0,

        # Exclude internal ADM since Chromium uses its own IO handling.
        'include_internal_audio_device%': 0,

        # Remove tests for Chromium to avoid slowing down GYP generation.
        'include_tests%': 0,
        'restrict_webrtc_logging%': 1,
      }, {  # Settings for the standalone (not-in-Chromium) build.
        'use_gtk%': 1,
        # TODO(andrew): For now, disable the Chrome plugins, which causes a
        # flood of chromium-style warnings. Investigate enabling them:
        # http://code.google.com/p/webrtc/issues/detail?id=163
        'clang_use_chrome_plugins%': 0,

        'include_pulse_audio%': 1,
        'include_internal_audio_device%': 1,
        'include_tests%': 1,
        'restrict_webrtc_logging%': 0,
      }],
      ['target_arch=="arm" or target_arch=="arm64" or target_arch=="mipsel"', {
        'prefer_fixed_point%': 1,
      }],
      ['(target_arch=="arm" and arm_neon==1) or target_arch=="arm64"', {
        'build_with_neon%': 1,
      }],
      ['OS!="ios" and (target_arch!="arm" or arm_version>=7) and target_arch!="mips64el"', {
        'rtc_use_openmax_dl%': 1,
      }, {
        'rtc_use_openmax_dl%': 0,
      }],
    ], # conditions
  },
  'target_defaults': {
    'conditions': [
      ['restrict_webrtc_logging==1', {
        'defines': ['WEBRTC_RESTRICT_LOGGING',],
      }],
      ['build_with_mozilla==1', {
        'defines': [
          # Changes settings for Mozilla build.
          'WEBRTC_MOZILLA_BUILD',
         ],
      }],
      ['have_dbus_glib==1', {
        'defines': [
          'HAVE_DBUS_GLIB',
         ],
         'cflags': [
           '<!@(pkg-config --cflags dbus-glib-1)',
         ],
      }],
      ['rtc_relative_path==1', {
        'defines': ['EXPAT_RELATIVE_PATH',],
      }],
      ['os_posix==1', {
        'configurations': {
          'Debug_Base': {
            'defines': [
              # Chromium's build/common.gypi defines _DEBUG for all posix
              # _except_ for ios & mac.  The size of data types such as
              # pthread_mutex_t varies between release and debug builds
              # and is controlled via this flag.  Since we now share code
              # between base/base.gyp and build/common.gypi (this file),
              # both gyp(i) files, must consistently set this flag uniformly
              # or else we'll run in to hard-to-figure-out problems where
              # one compilation unit uses code from another but expects
              # differently laid out types.
              # For WebRTC, we want it there as well, because ASSERT and
              # friends trigger off of it.
              '_DEBUG',
            ],
          },
        },
      }],
      ['build_with_chromium==1', {
        'defines': [
          # Changes settings for Chromium build.
          # TODO(kjellander): Cleanup unused ones and move defines closer to the
          # source when webrtc:4256 is completed.
          'ENABLE_EXTERNAL_AUTH',
          'FEATURE_ENABLE_SSL',
          'HAVE_OPENSSL_SSL_H',
          'HAVE_SCTP',
          'HAVE_SRTP',
          'HAVE_WEBRTC_VIDEO',
          'HAVE_WEBRTC_VOICE',
          'LOGGING_INSIDE_WEBRTC',
          'NO_MAIN_THREAD_WRAPPING',
          'NO_SOUND_SYSTEM',
          'SSL_USE_OPENSSL',
          'USE_WEBRTC_DEV_BRANCH',
          'WEBRTC_CHROMIUM_BUILD',
        ],
        'include_dirs': [
          # Include the top-level directory when building in Chrome, so we can
          # use full paths (e.g. headers inside testing/ or third_party/).
          '<(DEPTH)',
          # The overrides must be included before the WebRTC root as that's the
          # mechanism for selecting the override headers in Chromium.
          '../../webrtc_overrides',
          # The WebRTC root is needed to allow includes in the WebRTC code base
          # to be prefixed with webrtc/.
          '../..',
        ],
      }, {
         'includes': [
           # Rules for excluding e.g. foo_win.cc from the build on non-Windows.
           'filename_rules.gypi',
         ],
         # Include the top-level dir so the WebRTC code can use full paths.
        'include_dirs': [
          '../..',
        ],
        'conditions': [
          ['os_posix==1', {
            'conditions': [
              # -Wextra is currently disabled in Chromium's common.gypi. Enable
              # for targets that can handle it. For Android/arm64 right now
              # there will be an 'enumeral and non-enumeral type in conditional
              # expression' warning in android_tools/ndk_experimental's version
              # of stlport.
              # See: https://code.google.com/p/chromium/issues/detail?id=379699
              ['target_arch!="arm64" or OS!="android"', {
                'cflags': [
                  '-Wextra',
                  # We need to repeat some flags from Chromium's common.gypi
                  # here that get overridden by -Wextra.
                  '-Wno-unused-parameter',
                  '-Wno-missing-field-initializers',
                  '-Wno-strict-overflow',
                ],
              }],
            ],
            'cflags_cc': [
              '-Wnon-virtual-dtor',
              # This is enabled for clang; enable for gcc as well.
              '-Woverloaded-virtual',
            ],
          }],
          ['clang==1', {
            'cflags': [
              '-Wimplicit-fallthrough',
              '-Wthread-safety',
              '-Winconsistent-missing-override',
            ],
          }],
        ],
      }],
      ['target_arch=="arm64"', {
        'defines': [
          'WEBRTC_ARCH_ARM64',
          'WEBRTC_HAS_NEON',
        ],
      }],
      ['target_arch=="arm"', {
        'defines': [
          'WEBRTC_ARCH_ARM',
        ],
        'conditions': [
          ['arm_version>=7', {
            'defines': ['WEBRTC_ARCH_ARM_V7',],
            'conditions': [
              ['arm_neon==1', {
                'defines': ['WEBRTC_HAS_NEON',],
              }],
            ],
          }],
        ],
      }],
      ['target_arch=="mipsel" and mips_arch_variant!="r6"', {
        'defines': [
          'MIPS32_LE',
        ],
        'conditions': [
          ['mips_float_abi=="hard"', {
            'defines': [
              'MIPS_FPU_LE',
            ],
          }],
          ['mips_arch_variant=="r2"', {
            'defines': [
              'MIPS32_R2_LE',
            ],
          }],
          ['mips_dsp_rev==1', {
            'defines': [
              'MIPS_DSP_R1_LE',
            ],
          }],
          ['mips_dsp_rev==2', {
            'defines': [
              'MIPS_DSP_R1_LE',
              'MIPS_DSP_R2_LE',
            ],
          }],
        ],
      }],
      ['coverage==1 and OS=="linux"', {
        'cflags': [ '-ftest-coverage',
                    '-fprofile-arcs' ],
        'ldflags': [ '--coverage' ],
        'link_settings': { 'libraries': [ '-lgcov' ] },
      }],
     ['webrtc_sanitize_coverage!=""', {
        'cflags': [ '-fsanitize-coverage=<(webrtc_sanitize_coverage)' ],
        'ldflags': [ '-fsanitize-coverage=<(webrtc_sanitize_coverage)' ],
     }],
     ['webrtc_sanitize_coverage!="" and OS=="mac"', {
        'xcode_settings': {
            'OTHER_CFLAGS': [
               '-fsanitize-coverage=func',
            ],
         },
      }],
      ['os_posix==1', {
        # For access to standard POSIXish features, use WEBRTC_POSIX instead of
        # a more specific macro.
        'defines': [
          'WEBRTC_POSIX',
        ],
      }],
      ['OS=="ios"', {
        'defines': [
          'WEBRTC_MAC',
          'WEBRTC_IOS',
        ],
      }],
      ['OS=="ios" and ios_override_visibility==1', {
        'xcode_settings': {
          'GCC_INLINES_ARE_PRIVATE_EXTERN': 'NO',
          'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',
        }
      }],
      ['OS=="linux"', {
        'defines': [
          'WEBRTC_LINUX',
        ],
      }],
      ['OS=="mac"', {
        'defines': [
          'WEBRTC_MAC',
        ],
      }],
      ['OS=="win"', {
        'defines': [
          'WEBRTC_WIN',
        ],
        # TODO(andrew): enable all warnings when possible.
        # TODO(phoglund): get rid of 4373 supression when
        # http://code.google.com/p/webrtc/issues/detail?id=261 is solved.
        'msvs_disabled_warnings': [
          4373,  # legacy warning for ignoring const / volatile in signatures.
          4389,  # Signed/unsigned mismatch.
        ],
        # Re-enable some warnings that Chromium disables.
        'msvs_disabled_warnings!': [4189,],
      }],
      ['OS=="android"', {
        'defines': [
          'WEBRTC_LINUX',
          'WEBRTC_ANDROID',
         ],
         'conditions': [
           ['clang==0', {
             # The Android NDK doesn't provide optimized versions of these
             # functions. Ensure they are disabled for all compilers.
             'cflags': [
               '-fno-builtin-cos',
               '-fno-builtin-sin',
               '-fno-builtin-cosf',
               '-fno-builtin-sinf',
             ],
           }],
         ],
      }],
      ['chromeos==1', {
        'defines': [
          'CHROMEOS',
        ],
      }],
      ['os_bsd==1', {
        'defines': [
          'BSD',
        ],
      }],
      ['OS=="openbsd"', {
        'defines': [
          'OPENBSD',
        ],
      }],
      ['OS=="freebsd"', {
        'defines': [
          'FREEBSD',
        ],
      }],
      ['include_internal_audio_device==1', {
        'defines': [
          'WEBRTC_INCLUDE_INTERNAL_AUDIO_DEVICE',
        ],
      }],
      ['libvpx_build_vp9==0', {
        'defines': [
          'RTC_DISABLE_VP9',
        ],
      }],
    ], # conditions
    'direct_dependent_settings': {
      'conditions': [
        ['build_with_mozilla==1', {
          'defines': [
            # Changes settings for Mozilla build.
            'WEBRTC_MOZILLA_BUILD',
           ],
        }],
        ['build_with_chromium==1', {
          'defines': [
            # Changes settings for Chromium build.
            # TODO(kjellander): Cleanup unused ones and move defines closer to
            # the source when webrtc:4256 is completed.
            'FEATURE_ENABLE_SSL',
            'FEATURE_ENABLE_VOICEMAIL',
            'EXPAT_RELATIVE_PATH',
            'GTEST_RELATIVE_PATH',
            'NO_MAIN_THREAD_WRAPPING',
            'NO_SOUND_SYSTEM',
            'WEBRTC_CHROMIUM_BUILD',
          ],
          'include_dirs': [
            # The overrides must be included first as that is the mechanism for
            # selecting the override headers in Chromium.
            '../../webrtc_overrides',
            '../..',
          ],
        }, {
          'include_dirs': [
            '../..',
          ],
        }],
        ['OS=="mac"', {
          'defines': [
            'WEBRTC_MAC',
          ],
        }],
        ['OS=="ios"', {
          'defines': [
            'WEBRTC_MAC',
            'WEBRTC_IOS',
          ],
        }],
        ['OS=="win"', {
          'defines': [
            'WEBRTC_WIN',
            '_CRT_SECURE_NO_WARNINGS',  # Suppress warnings about _vsnprinf
          ],
        }],
        ['OS=="linux"', {
          'defines': [
            'WEBRTC_LINUX',
          ],
        }],
        ['OS=="android"', {
          'defines': [
            'WEBRTC_LINUX',
            'WEBRTC_ANDROID',
           ],
        }],
        ['os_posix==1', {
          # For access to standard POSIXish features, use WEBRTC_POSIX instead
          # of a more specific macro.
          'defines': [
            'WEBRTC_POSIX',
          ],
        }],
        ['chromeos==1', {
          'defines': [
            'CHROMEOS',
          ],
        }],
        ['os_bsd==1', {
          'defines': [
            'BSD',
          ],
        }],
        ['OS=="openbsd"', {
          'defines': [
            'OPENBSD',
          ],
        }],
        ['OS=="freebsd"', {
          'defines': [
            'FREEBSD',
          ],
        }],
      ],
    },
  }, # target_defaults
}
