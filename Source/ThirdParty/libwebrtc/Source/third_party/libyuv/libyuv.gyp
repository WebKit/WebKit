# Copyright 2011 The LibYuv Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS. All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'includes': [
    'libyuv.gypi',
  ],
  # Make sure that if we are being compiled to an xcodeproj, nothing tries to
  # include a .pch.
  'xcode_settings': {
    'GCC_PREFIX_HEADER': '',
    'GCC_PRECOMPILE_PREFIX_HEADER': 'NO',
  },
  'variables': {
    'use_system_libjpeg%': 0,
    'libyuv_disable_jpeg%': 0,
    # 'chromium_code' treats libyuv as internal and increases warning level.
    'chromium_code': 1,
    # clang compiler default variable usable by other apps that include libyuv.
    'clang%': 0,
    # Link-Time Optimizations.
    'use_lto%': 0,
    'mips_msa%': 0,  # Default to msa off.
    'build_neon': 0,
    'build_msa': 0,
    'conditions': [
       ['(target_arch == "armv7" or target_arch == "armv7s" or \
       (target_arch == "arm" and arm_version >= 7) or target_arch == "arm64")\
       and (arm_neon == 1 or arm_neon_optional == 1)', {
         'build_neon': 1,
       }],
       ['(target_arch == "mipsel" or target_arch == "mips64el")\
       and (mips_msa == 1)',
       {
         'build_msa': 1,
       }],
    ],
  },

  'targets': [
    {
      'target_name': 'libyuv',
      # Change type to 'shared_library' to build .so or .dll files.
      'type': 'static_library',
      'variables': {
        'optimize': 'max',  # enable O2 and ltcg.
      },
      # Allows libyuv.a redistributable library without external dependencies.
      'standalone_static_library': 1,
      'conditions': [
       # Disable -Wunused-parameter
        ['clang == 1', {
          'cflags': [
            '-Wno-unused-parameter',
         ],
        }],
        ['build_neon != 0', {
          'defines': [
            'LIBYUV_NEON',
          ],
          'cflags!': [
            '-mfpu=vfp',
            '-mfpu=vfpv3',
            '-mfpu=vfpv3-d16',
            # '-mthumb',  # arm32 not thumb
          ],
          'conditions': [
            # Disable LTO in libyuv_neon target due to gcc 4.9 compiler bug.
            ['clang == 0 and use_lto == 1', {
              'cflags!': [
                '-flto',
                '-ffat-lto-objects',
              ],
            }],
            # arm64 does not need -mfpu=neon option as neon is not optional
            ['target_arch != "arm64"', {
              'cflags': [
                '-mfpu=neon',
                # '-marm',  # arm32 not thumb
              ],
            }],
          ],
        }],
        ['build_msa != 0', {
          'defines': [
            'LIBYUV_MSA',
          ],
        }],
        ['OS != "ios" and libyuv_disable_jpeg != 1', {
          'defines': [
            'HAVE_JPEG'
          ],
          'conditions': [
            # Caveat system jpeg support may not support motion jpeg
            [ 'use_system_libjpeg == 1', {
              'dependencies': [
                 '<(DEPTH)/third_party/libjpeg/libjpeg.gyp:libjpeg',
              ],
            }, {
              'dependencies': [
                 '<(DEPTH)/third_party/libjpeg_turbo/libjpeg.gyp:libjpeg',
              ],
            }],
            [ 'use_system_libjpeg == 1', {
              'link_settings': {
                'libraries': [
                  '-ljpeg',
                ],
              }
            }],
          ],
        }],
      ], #conditions
      'defines': [
        # Enable the following 3 macros to turn off assembly for specified CPU.
        # 'LIBYUV_DISABLE_X86',
        # 'LIBYUV_DISABLE_NEON',
        # 'LIBYUV_DISABLE_MIPS',
        # Enable the following macro to build libyuv as a shared library (dll).
        # 'LIBYUV_USING_SHARED_LIBRARY',
        # TODO(fbarchard): Make these into gyp defines.
      ],
      'include_dirs': [
        'include',
        '.',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'include',
          '.',
        ],
        'conditions': [
          ['OS == "android" and target_arch == "arm64"', {
            'ldflags': [
              '-Wl,--dynamic-linker,/system/bin/linker64',
            ],
          }],
          ['OS == "android" and target_arch != "arm64"', {
            'ldflags': [
              '-Wl,--dynamic-linker,/system/bin/linker',
            ],
          }],
        ], #conditions
      },
      'sources': [
        '<@(libyuv_sources)',
      ],
    },
  ], # targets.
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
