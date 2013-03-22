#
# Copyright (C) 2013 Igalia S.L.
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
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'GenerateInjectedScriptSource',
#   'type': 'none',
#   'variables': {
#     'input_file_path': '../inspector/InjectedScriptSource.js'',
#     'output_file_path': '<(SHARED_INTERMEDIATE_DIR)/WebCore/InjectedScriptSource.h',
#     'character_array_name': 'InjectedScriptSource_js',
#   },
#   'includes': [ 'ConvertFileToHeaderWithCharacterArray.gypi' ],
# },
{
  'actions': [
    {
      'action_name': 'ConvertFileToHeaderWithCharacterArray',
      'inputs': [
        '<(WebCore)/inspector/xxd.pl',
        '<(input_file_path)',
      ],
      'outputs': [ '<@(output_file_path)', ],
      'action': [
        '<(PERL)',
        '<(WebCore)/inspector/xxd.pl',
        '<(character_array_name)',
        '<(input_file_path)',
        '<@(_outputs)'
      ],
      'message': 'Generating <(output_file_path) from <(input_file_path)',
    },
  ],

  # Since this target generates header files, it needs to be a hard dependency.
  'hard_dependency': 1,
}
