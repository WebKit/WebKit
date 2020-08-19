# Copyright (C) 2010 Chris Jerdonek (cjerdonek@webkit.org)
# Copyright (C) 2019, 2020 Apple Inc. All rights reserved.
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

import re
import sys

from webkitcorepy import StringIO

from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.thirdparty.autoinstalled import pycodestyle

from webkitcorepy import OutputCapture

class PythonChecker(object):
    """Processes text lines for checking style."""
    def __init__(self, file_path, handle_style_error):
        self._file_path = file_path
        self._handle_style_error = handle_style_error

    def check(self, lines):
        self._check_pycodestyle(lines)
        # FIXME: https://bugs.webkit.org/show_bug.cgi?id=204133
        # Pylint can't live happily in python 2 and 3 world, we need to pick one
        if sys.version_info < (3, 0):
            self._check_pylint(lines)

    def _check_pycodestyle(self, lines):
        # Initialize pycodestyle.options, which is necessary for
        # Checker.check_all() to execute.
        pycodestyle.process_options(arglist=[self._file_path])

        pycodestyle_checker = pycodestyle.Checker(self._file_path)

        def _pycodestyle_handle_error(line_number, offset, text, check):
            # FIXME: Incorporate the character offset into the error output.
            #        This will require updating the error handler __call__
            #        signature to include an optional "offset" parameter.
            pycodestyle_code = text[:4]
            pycodestyle_message = text[5:]

            category = "pep8/" + pycodestyle_code

            self._handle_style_error(line_number, category, 5, pycodestyle_message)

        pycodestyle_checker.report_error = _pycodestyle_handle_error
        pycodestyle_errors = pycodestyle_checker.check_all()

    def _check_pylint(self, lines):
        pylinter = Pylinter()

        # FIXME: for now, we only report pylint errors, but we should be catching and
        # filtering warnings using the rules in style/checker.py instead.
        output = pylinter.run(['-E', self._file_path])

        lint_regex = re.compile(r'([^:]+):([^:]+): \[([^]]+)\] (.*)')
        for error in output.getvalue().splitlines():
            match_obj = lint_regex.match(error)
            if not match_obj:
                continue
            line_number = int(match_obj.group(2))
            category_and_method = match_obj.group(3).split(', ')
            category = 'pylint/' + (category_and_method[0])
            if len(category_and_method) > 1:
                message = '[%s] %s' % (category_and_method[1], match_obj.group(4))
            else:
                message = match_obj.group(4)
            self._handle_style_error(line_number, category, 5, message)


class Pylinter(object):
    # We filter out these messages because they are bugs in pylint that produce false positives.
    # FIXME: Does it make sense to combine these rules with the rules in style/checker.py somehow?
    FALSE_POSITIVES = [
        # possibly http://www.logilab.org/ticket/98613 ?
        "Instance of 'Popen' has no 'poll' member",
        "Instance of 'Popen' has no 'returncode' member",
        "Instance of 'Popen' has no 'stdin' member",
        "Instance of 'Popen' has no 'stdout' member",
        "Instance of 'Popen' has no 'stderr' member",
        "Instance of 'Popen' has no 'wait' member",
        "Instance of 'Popen' has no 'pid' member",
    ]

    def __init__(self):
        self._pylintrc = WebKitFinder(FileSystem()).path_from_webkit_base('Tools', 'Scripts', 'webkitpy', 'pylintrc')

    def run(self, argv):
        output = _FilteredStringIO(self.FALSE_POSITIVES)
        with OutputCapture():
            from webkitpy.thirdparty.autoinstalled.pylint import lint
            from webkitpy.thirdparty.autoinstalled.pylint.reporters.text import ParseableTextReporter
            lint.Run(['--rcfile', self._pylintrc] + argv, reporter=ParseableTextReporter(output=output), exit=False)
        return output


class _FilteredStringIO(StringIO):
    def __init__(self, bad_messages):
        StringIO.__init__(self)
        self.dropped_last_msg = False
        self.bad_messages = bad_messages

    def write(self, msg=''):
        if not self._filter(msg):
            StringIO.write(self, msg)

    def _filter(self, msg):
        if any(bad_message in msg for bad_message in self.bad_messages):
            self.dropped_last_msg = True
            return True
        if self.dropped_last_msg:
            # We drop the newline after a dropped message as well.
            self.dropped_last_msg = False
            if msg == '\n':
                return True
        return False


class Python3Checker(object):
    def __init__(self, file_path, handle_style_error):
        self._file_path = file_path
        self._handle_style_error = handle_style_error

    def check(self, lines):
        from webkitpy.thirdparty.autoinstalled import pycodestyle

        def handler(line_number, offset, text, check):
            # Text is of the form 'E### <description of error>'
            code = text[:4]
            message = text[5:]
            category = "pycodestyle/" + code
            self._handle_style_error(
                line_number=line_number,
                category=category,
                confidence=5,
                message=message,
            )

        checker = pycodestyle.Checker(self._file_path)
        checker.report_error = handler
        checker.check_all()
