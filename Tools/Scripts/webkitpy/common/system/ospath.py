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

"""Contains a substitute for Python 2.6's os.path.relpath()."""

import os


# This function is a replacement for os.path.relpath(), which is only
# available in Python 2.6:
#
# http://docs.python.org/library/os.path.html#os.path.relpath
#
# It should behave essentially the same as os.path.relpath(), except for
# returning None on paths not contained in abs_start_path.
def relpath(path, start_path, os_path_abspath=None, sep=None):
    """Return a path relative to the given start path, or None.

    Returns None if the path is not contained in the directory start_path.

    Args:
      path: An absolute or relative path to convert to a relative path.
      start_path: The path relative to which the given path should be
                  converted.
      os_path_abspath: A replacement function for unit testing.  This
                       function should strip trailing slashes just like
                       os.path.abspath().  Defaults to os.path.abspath.
      sep: Path separator.  Defaults to os.path.sep

    """
    if os_path_abspath is None:
        os_path_abspath = os.path.abspath
    sep = sep or os.sep

    # Since os_path_abspath() calls os.path.normpath()--
    #
    # (see http://docs.python.org/library/os.path.html#os.path.abspath )
    #
    # it also removes trailing slashes and converts forward and backward
    # slashes to the preferred slash os.sep.
    start_path = os_path_abspath(start_path)
    path = os_path_abspath(path)

    if not path.lower().startswith(start_path.lower()):
        # Then path is outside the directory given by start_path.
        return None

    rel_path = path[len(start_path):]

    if not rel_path:
        # Then the paths are the same.
        pass
    elif rel_path[0] == sep:
        # It is probably sufficient to remove just the first character
        # since os.path.normpath() collapses separators, but we use
        # lstrip() just to be sure.
        rel_path = rel_path.lstrip(sep)
    else:
        # We are in the case typified by the following example:
        #
        # start_path = "/tmp/foo"
        # path = "/tmp/foobar"
        # rel_path = "bar"
        return None

    return rel_path
