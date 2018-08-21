# Copyright 2011 The LibYuv Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS. All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'variables': {
    # Can be enabled if your jpeg has GYP support.
    'libyuv_disable_jpeg%': 1,
    'mips_msa%': 0,  # Default to msa off.
  },
  'targets': [
    {
      'target_name': 'libyuv_unittest',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'libyuv.gyp:libyuv',
        'testing/gtest.gyp:gtest',
        'third_party/gflags/gflags.gyp:gflags',
      ],
      'direct_dependent_settings': {
        'defines': [
          'GTEST_RELATIVE_PATH',
        ],
      },
      'export_dependent_settings': [
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'sources': [
        # headers
        'unit_test/unit_test.h',

        # sources
        'unit_test/basictypes_test.cc',
        'unit_test/compare_test.cc',
        'unit_test/color_test.cc',
        'unit_test/convert_test.cc',
        'unit_test/cpu_test.cc',
        'unit_test/cpu_thread_test.cc',
        'unit_test/math_test.cc',
        'unit_test/planar_test.cc',
        'unit_test/rotate_argb_test.cc',
        'unit_test/rotate_test.cc',
        'unit_test/scale_argb_test.cc',
        'unit_test/scale_test.cc',
        'unit_test/unit_test.cc',
        'unit_test/video_common_test.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'cflags': [
            '-fexceptions',
          ],
        }],
        [ 'OS == "ios"', {
          'xcode_settings': {
            'DEBUGGING_SYMBOLS': 'YES',
            'DEBUG_INFORMATION_FORMAT' : 'dwarf-with-dsym',
            # Work around compile issue with isosim.mm, see
            # https://code.google.com/p/libyuv/issues/detail?id=548 for details.
            'WARNING_CFLAGS': [
              '-Wno-sometimes-uninitialized',
            ],
          },
          'cflags': [
            '-Wno-sometimes-uninitialized',
          ],
        }],
        [ 'OS != "ios" and libyuv_disable_jpeg != 1', {
          'defines': [
            'HAVE_JPEG',
          ],
        }],
        ['OS=="android"', {
          'dependencies': [
            '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
        # TODO(YangZhang): These lines can be removed when high accuracy
        # YUV to RGB to Neon is ported.
        [ '(target_arch == "armv7" or target_arch == "armv7s" \
          or (target_arch == "arm" and arm_version >= 7) \
          or target_arch == "arm64") \
          and (arm_neon == 1 or arm_neon_optional == 1)', {
          'defines': [
            'LIBYUV_NEON'
          ],
        }],
        [ '(target_arch == "mipsel" or target_arch == "mips64el") \
          and (mips_msa == 1)', {
          'defines': [
            'LIBYUV_MSA'
          ],
        }],
      ], # conditions
      'defines': [
        # Enable the following 3 macros to turn off assembly for specified CPU.
        # 'LIBYUV_DISABLE_X86',
        # 'LIBYUV_DISABLE_NEON',
        # Enable the following macro to build libyuv as a shared library (dll).
        # 'LIBYUV_USING_SHARED_LIBRARY',
      ],
    },
    {
      'target_name': 'compare',
      'type': 'executable',
      'dependencies': [
        'libyuv.gyp:libyuv',
      ],
      'sources': [
        # sources
        'util/compare.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'cflags': [
            '-fexceptions',
          ],
        }],
      ], # conditions
    },
    {
      'target_name': 'yuvconvert',
      'type': 'executable',
      'dependencies': [
        'libyuv.gyp:libyuv',
      ],
      'sources': [
        # sources
        'util/yuvconvert.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'cflags': [
            '-fexceptions',
          ],
        }],
      ], # conditions
    },
    # TODO(fbarchard): Enable SSE2 and OpenMP for better performance.
    {
      'target_name': 'psnr',
      'type': 'executable',
      'sources': [
        # sources
        'util/psnr_main.cc',
        'util/psnr.cc',
        'util/ssim.cc',
      ],
      'dependencies': [
        'libyuv.gyp:libyuv',
      ],
      'conditions': [
        [ 'OS != "ios" and libyuv_disable_jpeg != 1', {
          'defines': [
            'HAVE_JPEG',
          ],
        }],
      ], # conditions
    },

    {
      'target_name': 'cpuid',
      'type': 'executable',
      'sources': [
        # sources
        'util/cpuid.c',
      ],
      'dependencies': [
        'libyuv.gyp:libyuv',
      ],
    },
  ], # targets
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'yuv_unittest_apk',
          'type': 'none',
          'variables': {
            'test_suite_name': 'yuv_unittest',
            'input_shlib_path': '<(SHARED_LIB_DIR)/(SHARED_LIB_PREFIX)libyuv_unittest<(SHARED_LIB_SUFFIX)',
          },
          'includes': [
            'build/apk_test.gypi',
          ],
          'dependencies': [
            'libyuv_unittest',
          ],
        },
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
