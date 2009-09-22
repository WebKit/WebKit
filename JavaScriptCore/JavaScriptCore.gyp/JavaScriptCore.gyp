#
# Copyright (C) 2009 Google Inc. All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

{
  'includes': [
    # FIXME: Sense whether upstream or downstream build, and
    # include the right features.gypi
    '../../WebKit/chromium/features.gypi',
    '../JavaScriptCore.gypi',
  ],
  'variables': {
    # FIXME: Sense whether upstream or downstream build, and
    # point to the right src dir
    'chromium_src_dir': '../../../..',
  },
  'targets': [
    {
      # This target sets up defines and includes that are required by WTF and
      # its dependents.
      'target_name': 'wtf_config',
      'type': 'none',
      'msvs_guid': '2E2D3301-2EC4-4C0F-B889-87073B30F673',
      'direct_dependent_settings': {
        'defines': [
          # Import features_defines from features.gypi
          '<@(feature_defines)',
          
          # Turns on #if PLATFORM(CHROMIUM)
          'BUILDING_CHROMIUM__=1',
          # Controls wtf/FastMalloc
          # FIXME: consider moving into config.h
          'USE_SYSTEM_MALLOC=1',
        ],
        'conditions': [
          ['OS=="win"', {
            'defines': [
              '__STD_C',
              '_CRT_SECURE_NO_DEPRECATE',
              '_SCL_SECURE_NO_DEPRECATE',
              'CRASH=__debugbreak',
            ],
            'include_dirs': [
              '../os-win32',
              '<(chromium_src_dir)/webkit/build/JavaScriptCore',
            ],
          }],
          ['OS=="mac"', {
            'defines': [
              # Ensure that only Leopard features are used when doing the
              # Mac build.
              'BUILDING_ON_LEOPARD',

              # Use USE_NEW_THEME on Mac.
              'WTF_USE_NEW_THEME=1',
            ],
          }],
          ['OS=="linux" or OS=="freebsd"', {
            'defines': [
              'WTF_USE_PTHREADS=1',
            ],
          }],
        ],
      }
    },
    {
      'target_name': 'wtf',
      'type': '<(library)',
      'msvs_guid': 'AA8A5A85-592B-4357-BC60-E0E91E026AF6',
      'dependencies': [
        'wtf_config',
        '<(chromium_src_dir)/third_party/icu/icu.gyp:icui18n',
        '<(chromium_src_dir)/third_party/icu/icu.gyp:icuuc',
      ],
      'include_dirs': [
        '../',
        '../wtf',
        '../wtf/unicode',
      ],
      'sources': [
        '<@(javascriptcore_files)',
      ],
      'sources/': [
        # First exclude everything ...
        ['exclude', '../'],
        # ... Then include what we want.
        ['include', '../wtf/'],
        # GLib/GTK, even though its name doesn't really indicate.
        ['exclude', '/(GOwnPtr|glib/.*)\\.(cpp|h)$'],
        ['exclude', '(Default|Gtk|Mac|None|Qt|Win|Wx)\\.(cpp|mm)$'],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../',
          '../wtf',
        ],
      },
      'export_dependent_settings': [
        'wtf_config',
        '<(chromium_src_dir)/third_party/icu/icu.gyp:icui18n',
        '<(chromium_src_dir)/third_party/icu/icu.gyp:icuuc',
      ],
      'msvs_disabled_warnings': [4127, 4355, 4510, 4512, 4610, 4706],
      'conditions': [
        ['OS=="win"', {
          'sources/': [
            ['exclude', 'ThreadingPthreads\\.cpp$'],
            ['include', 'Thread(ing|Specific)Win\\.cpp$']
          ],
          'include_dirs': [
            '<(chromium_src_dir)/webkit/build',
            '../kjs',
            '../API',
            # These 3 do not seem to exist.
            '../bindings',
            '../bindings/c',
            '../bindings/jni',
            # FIXME: removed these - don't seem to exist
            'pending',
            'pending/wtf',
          ],
          'include_dirs!': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit',
          ],
        }],
      ],
    },
    {
      'target_name': 'pcre',
      'type': '<(library)',
      'dependencies': [
        'wtf',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': ['<(chromium_src_dir)/build/win/system.gyp:cygwin'],
        }],
      ],
      'msvs_guid': '49909552-0B0C-4C14-8CF6-DB8A2ADE0934',
      'actions': [
        {
          'action_name': 'dftables',
          'inputs': [
            '../pcre/dftables',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/chartables.c',
          ],
          'action': ['perl', '-w', '<@(_inputs)', '<@(_outputs)'],
        },
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
      ],
      'sources': [
        '<@(javascriptcore_files)',
      ],
      'sources/': [
        # First exclude everything ...
        ['exclude', '../'],
        # ... Then include what we want.
        ['include', '../pcre/'],
        # ucptable.cpp is #included by pcre_ucp_searchfunchs.cpp and is not
        # intended to be compiled directly.
        ['exclude', '../pcre/ucptable.cpp$'],
      ],
      'export_dependent_settings': [
        'wtf',
      ],
    },
  ], # targets
}
