#
# Copyright (C) 2012 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#         * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#         * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#         * Neither the name of Google Inc. nor the names of its
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

# This skia_webkit target is a dependency of Chromium's skia/skia.gyp.
# It only contains code suppressions which keep Webkit tests from failing.
{
  'targets': [
    {
      'target_name': 'skia_webkit',
      'type': 'none',
      'direct_dependent_settings': {
        'defines': [
          # Place defines here that require significant WebKit rebaselining, or that
          # are otherwise best removed in WebKit and then rolled into Chromium.
          # Defines should be in single quotes and a comma must appear after every one.
          # DO NOT remove the define until you are ready to rebaseline, and
          # AFTER the flag has been removed from skia.gyp in Chromium.

          'SK_DISABLE_DITHER_32BIT_GRADIENT',
          'SK_IGNORE_QUAD_STROKE_FIX',
          'SK_DISABLE_DASHING_OPTIMIZATION',

          # The following change is not ready to be enabled due to uncertainty about its effect.
          # Consult with the Skia team before removing.
          'SK_IGNORE_TREAT_AS_SPRITE',

        ],
      },
    },
  ],
}
