# Copyright (C) 2010 Chris Jerdonek (cjerdonek@webkit.org)
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Supports checking WebKit style in Python files."""

from ...style_references import pep8


class PythonChecker(object):

    """Processes text lines for checking style."""

    def __init__(self, file_path, handle_style_error):
        self._file_path = file_path
        self._handle_style_error = handle_style_error

    def check(self, lines):
        # Initialize pep8.options, which is necessary for
        # Checker.check_all() to execute.
        pep8.process_options(arglist=[self._file_path])

        checker = pep8.Checker(self._file_path)

        def _pep8_handle_error(line_number, offset, text, check):
            # FIXME: Incorporate the character offset into the error output.
            #        This will require updating the error handler __call__
            #        signature to include an optional "offset" parameter.
            pep8_code = text[:4]
            pep8_message = text[5:]

            category = "pep8/" + pep8_code

            self._handle_style_error(line_number, category, 5, pep8_message)

        checker.report_error = _pep8_handle_error

        errors = checker.check_all()
