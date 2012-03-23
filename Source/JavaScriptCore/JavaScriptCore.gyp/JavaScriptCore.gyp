#
# Copyright (C) 2009, 2012 Google Inc. All rights reserved.
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
    '../../WebKit/chromium/WinPrecompile.gypi',
    '../../WebKit/chromium/features.gypi',
    '../JavaScriptCore.gypi',
  ],
  'variables': {
    # Location of the chromium src directory.
    'conditions': [
      ['inside_chromium_build==0', {
        # Webkit is being built outside of the full chromium project.
        'chromium_src_dir': '../../WebKit/chromium',
      },{
        # WebKit is checked out in src/chromium/third_party/WebKit
        'chromium_src_dir': '../../../../..',
      }],
    ],
  },
  'conditions': [
    ['os_posix == 1 and OS != "mac" and OS != "android" and gcc_version==46', {
      'target_defaults': {
        # Disable warnings about c++0x compatibility, as some names (such as nullptr) conflict
        # with upcoming c++0x types.
        'cflags_cc': ['-Wno-c++0x-compat'],
      },
    }],
  ],
  'targets': [
    {
      'target_name': 'yarr',
      'type': 'static_library',
      'dependencies': [
        '../../WTF/WTF.gyp/WTF.gyp:wtf',
      ],
      'variables': { 'optimize': 'max' },
      'actions': [
        {
          'action_name': 'retgen',
          'inputs': [
            '../create_regex_tables',
          ],
          'arguments': [
            '--no-tables',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/RegExpJitTables.h',
          ],
          'action': ['python', '<@(_inputs)', '<@(_arguments)', '<@(_outputs)'],
        },
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '..',
        '../runtime',
      ],
      'sources': [
        '<@(javascriptcore_files)',
      ],
      'sources/': [
        # First exclude everything ...
        ['exclude', '../'],
        # ... Then include what we want.
        ['include', '../yarr/'],
        # The Yarr JIT isn't used in WebCore.
        ['exclude', '../yarr/YarrJIT\\.(h|cpp)$'],
      ],
      'export_dependent_settings': [
        '../../WTF/WTF.gyp/WTF.gyp:wtf',
      ],
    },
  ], # targets
}
