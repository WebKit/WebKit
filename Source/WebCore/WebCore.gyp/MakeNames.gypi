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
#{
#  'target_name': 'HTMLNames',
#  'type': 'none',
#  'variables': {
#    'make_names_inputs': [
#      '<(WebCore)/html/HTMLTagNames.in',
#      '<(WebCore)/html/HTMLAttributeNames.in',
#    ],
#    'make_names_outputs': [
#      '<(SHARED_INTERMEDIATE_DIR)/WebCore/HTMLNames.cpp',
#      '<(SHARED_INTERMEDIATE_DIR)/WebCore/HTMLNames.h',
#      '<(SHARED_INTERMEDIATE_DIR)/WebCore/HTMLElementFactory.cpp',
#      '<(SHARED_INTERMEDIATE_DIR)/WebCore/bindings/JSHTMLElementWrapperFactory.cpp',
#      '<(SHARED_INTERMEDIATE_DIR)/WebCore/bindings/JSHTMLElementWrapperFactory.h',
#    ],
#    'make_names_extra_arguments': [ '--factory', '--wrapperFactory' ],
#  },
#  'includes': [ 'MakeNames.gypi' ],
#},
{
  'actions': [
    {
      'action_name': 'GenerateNames',
      'inputs': [
        '<(WebCore)/dom/make_names.pl',
        '<@(make_names_inputs)',
      ],
      'outputs': [ '<@(make_names_outputs)', ],
      'action': [
        '<(PYTHON)',
        'scripts/action_makenames.py',
        '<@(_outputs)',
        '--',
        '<@(_inputs)',
        '--',
        '--extraDefines', '<(feature_defines)',
        '<@(make_names_extra_arguments)',
      ],
    },
  ],

  # Since this target generates header files, it needs to be a hard dependency.
  'hard_dependency': 1,
}
