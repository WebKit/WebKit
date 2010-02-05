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

"""Defines style error handler classes.

A style error handler is a function to call when a style error is
found. Style error handlers can also have state. A class that represents
a style error handler should implement the following methods.

Methods:

  __call__(self, line_number, category, confidence, message):

    Handle the occurrence of a style error.

    Check whether the error is reportable. If so, increment the total
    error count and report the details. Note that error reporting can
    be suppressed after reaching a certain number of reports.

    Args:
      line_number: The integer line number of the line containing the error.
      category: The name of the category of the error, for example
                "whitespace/newline".
      confidence: An integer between 1-5 that represents the level of
                  confidence in the error. The value 5 means that we are
                  certain of the problem, and the value 1 means that it
                  could be a legitimate construct.
      message: The error message to report.

"""


import sys


class DefaultStyleErrorHandler(object):

    """The default style error handler."""

    def __init__(self, file_path, options, increment_error_count,
                 stderr_write=None):
        """Create a default style error handler.

        Args:
          file_path: The path to the file containing the error. This
                     is used for reporting to the user.
          options: A ProcessorOptions instance.
          increment_error_count: A function that takes no arguments and
                                 increments the total count of reportable
                                 errors.
          stderr_write: A function that takes a string as a parameter
                        and that is called when a style error occurs.
                        Defaults to sys.stderr.write. This should be
                        used only for unit tests.

        """
        if stderr_write is None:
            stderr_write = sys.stderr.write

        self._file_path = file_path
        self._increment_error_count = increment_error_count
        self._options = options
        self._stderr_write = stderr_write

        # A string to integer dictionary cache of the number of reportable
        # errors per category passed to this instance.
        self._category_totals = {}

    def _add_reportable_error(self, category):
        """Increment the error count and return the new category total."""
        self._increment_error_count() # Increment the total.

        # Increment the category total.
        if not category in self._category_totals:
            self._category_totals[category] = 1
        else:
            self._category_totals[category] += 1

        return self._category_totals[category]

    def _max_reports(self, category):
        """Return the maximum number of errors to report."""
        if not category in self._options.max_reports_per_category:
            return None
        return self._options.max_reports_per_category[category]

    def __call__(self, line_number, category, confidence, message):
        """Handle the occurrence of a style error.

        See the docstring of this module for more information.

        """
        if not self._options.is_reportable(category,
                                           confidence,
                                           self._file_path):
            return

        category_total = self._add_reportable_error(category)

        max_reports = self._max_reports(category)

        if (max_reports is not None) and (category_total > max_reports):
            # Then suppress displaying the error.
            return

        if self._options.output_format == 'vs7':
            format_string = "%s(%s):  %s  [%s] [%d]\n"
        else:
            format_string = "%s:%s:  %s  [%s] [%d]\n"

        if category_total == max_reports:
            format_string += ("Suppressing further [%s] reports for this "
                              "file.\n" % category)

        self._stderr_write(format_string % (self._file_path,
                                            line_number,
                                            message,
                                            category,
                                            confidence))


class PatchStyleErrorHandler(object):

    """The style error function for patch files."""

    def __init__(self, diff, file_path, options, increment_error_count,
                 stderr_write):
        """Create a patch style error handler for the given path.

        Args:
          diff: A DiffFile instance.
          Other arguments: see the DefaultStyleErrorHandler.__init__()
                           documentation for the other arguments.

        """
        self._diff = diff
        self._default_error_handler = DefaultStyleErrorHandler(file_path,
                                          options,
                                          increment_error_count,
                                          stderr_write)

        # The line numbers of the modified lines. This is set lazily.
        self._line_numbers = set()

    def _get_line_numbers(self):
        """Return the line numbers of the modified lines."""
        if not self._line_numbers:
            for line in self._diff.lines:
                # When deleted line is not set, it means that
                # the line is newly added (or modified).
                if not line[0]:
                    self._line_numbers.add(line[1])

        return self._line_numbers

    def __call__(self, line_number, category, confidence, message):
        """Handle the occurrence of a style error.

        This function does not report errors occurring in lines not
        marked as modified or added in the patch.

        See the docstring of this module for more information.

        """
        if line_number not in self._get_line_numbers():
            # Then the error is not reportable.
            return

        self._default_error_handler(line_number, category, confidence,
                                    message)

