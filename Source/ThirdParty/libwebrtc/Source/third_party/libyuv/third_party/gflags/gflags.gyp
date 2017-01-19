#
# Copyright 2014 The LibYuv Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS. All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This is a copy of WebRTC's gflags.gyp.

{
  'variables': {
    'gflags_root': '<(DEPTH)/third_party/gflags',
    'conditions': [
      ['OS=="win"', {
        'gflags_gen_arch_root': '<(gflags_root)/gen/win',
      }, {
        'gflags_gen_arch_root': '<(gflags_root)/gen/posix',
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'gflags',
      'type': 'static_library',
      'include_dirs': [
        '<(gflags_gen_arch_root)/include/gflags',  # For configured files.
        '<(gflags_gen_arch_root)/include/private',  # For config.h
        '<(gflags_root)/src/src',  # For everything else.
      ],
      'defines': [
        # These macros exist so flags and symbols are properly
        # exported when building DLLs. Since we don't build DLLs, we
        # need to disable them.
        'GFLAGS_DLL_DECL=',
        'GFLAGS_DLL_DECLARE_FLAG=',
        'GFLAGS_DLL_DEFINE_FLAG=',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(gflags_gen_arch_root)/include',  # For configured files.
          '<(gflags_root)/src/src',  # For everything else.
        ],
        'defines': [
          'GFLAGS_DLL_DECL=',
          'GFLAGS_DLL_DECLARE_FLAG=',
          'GFLAGS_DLL_DEFINE_FLAG=',
        ],
      },
      'sources': [
        'src/src/gflags.cc',
        'src/src/gflags_completions.cc',
        'src/src/gflags_reporting.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'src/src/windows_port.cc',
          ],
          'msvs_disabled_warnings': [
            4005,  # WIN32_LEAN_AND_MEAN redefinition.
            4267,  # Conversion from size_t to "type".
          ],
          'configurations': {
            'Common_Base': {
              'msvs_configuration_attributes': {
                'CharacterSet': '2',  # Use Multi-byte Character Set.
              },
            },
          },
        }],
        # TODO(andrew): Look into fixing this warning upstream:
        # http://code.google.com/p/webrtc/issues/detail?id=760
        ['OS=="win" and clang==1', {
          'msvs_settings': {
            'VCCLCompilerTool': {
              'AdditionalOptions': [
                '-Wno-microsoft-include',
              ],
            },
          },
        }],
        ['clang==1', {
          'cflags': [
            '-Wno-microsoft-include',
          ],
        }],
      ],
    },
  ],
}
