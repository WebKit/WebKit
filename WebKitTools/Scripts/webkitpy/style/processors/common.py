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

"""Supports style checking not specific to any one processor."""


# FIXME: Test this list in the same way that the list of CppProcessor
#        categories is tested, for example by checking that all of its
#        elements appear in the unit tests. This should probably be done
#        after moving the relevant cpp_unittest.ErrorCollector code
#        into a shared location and refactoring appropriately.
categories = set([
    "whitespace/carriage_return",
])


def check_no_carriage_return(line, line_number, error):
    """Check that a line does not end with a carriage return.

    Returns true if the check is successful (i.e. if the line does not
    end with a carriage return), and false otherwise.

    Args:
      line: A string that is the line to check.
      line_number: The line number.
      error: The function to call with any errors found.

    """

    if line.endswith("\r"):
        error(line_number,
              "whitespace/carriage_return",
              1,
              "One or more unexpected \\r (^M) found; "
              "better to use only a \\n")
        return False

    return True


