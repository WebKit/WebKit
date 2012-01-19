# Copyright (C) 2012 Google Inc. All rights reserved.
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

# Include this file to make targets in your .gyp use the default precompiled
# header on Windows, when precompiled headers are turned on.

{
  'conditions': [
      ['OS=="win" and chromium_win_pch==1', {
          'variables': {
              'conditions': [
                  # We need to calculate the path to the gyp directory differently depending on whether we are
                  # being built stand-alone (via build-webkit --chromium) or as part of the Chromium checkout.
                  ['inside_chromium_build==0', {
                      'win_pch_dir': '<(DEPTH)/../../WebKit/chromium',
                  },{
                      'win_pch_dir': '<(DEPTH)/third_party/WebKit/Source/WebKit/chromium',
                  }],
              ]
          },
          'target_defaults': {
              'msvs_precompiled_header': '<(win_pch_dir)/WinPrecompile.h',
              'msvs_precompiled_source': '<(win_pch_dir)/WinPrecompile.cpp',
              'sources': ['<(win_pch_dir)/WinPrecompile.cpp'],
          }
      }],
  ],
}
