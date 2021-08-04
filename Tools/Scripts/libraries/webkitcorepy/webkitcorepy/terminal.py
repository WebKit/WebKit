# Copyright (C) 2021 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import sys


class Terminal(object):
    @classmethod
    def input(cls, *args, **kwargs):
        return (input if sys.version_info > (3, 0) else raw_input)(*args, **kwargs)

    @classmethod
    def choose(cls, prompt, options=None, default=None, strict=False, numbered=False):
        options = options or ('Yes', 'No')

        response = None
        while response is None:
            if numbered:
                numbered_options = ['{}) {}'.format(i + 1, options[i]) for i in range(len(options))]
                response = cls.input('{}:\n    {}\n: '.format(prompt, '\n    '.join(numbered_options)))
            else:
                response = cls.input('{} ({}): '.format(prompt, '/'.join(options)))

            if numbered and response.isdigit():
                index = int(response) - 1
                if index >= 0 and index < len(options):
                    response = options[index]

            if not strict:
                for option in options:
                    if option.lower().startswith(response.lower()):
                        response = option
                        break

            if response not in options:
                if not default:
                    print("'{}' is not an option".format(response))
                response = default

        return response
