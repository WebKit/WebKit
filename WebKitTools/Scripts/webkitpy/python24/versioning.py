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

"""Supports Python version checking."""

import logging
import sys

_log = logging.getLogger("webkitpy.python24.versioning")

# The minimum Python version the webkitpy package supports.
_MINIMUM_SUPPORTED_PYTHON_VERSION = "2.5"


def compare_version(sysmodule=None, target_version=None):
    """Compare the current Python version with a target version.

    Args:
      sysmodule: An object with version and version_info data attributes
                 used to detect the current Python version.  The attributes
                 should have the same semantics as sys.version and
                 sys.version_info.  This parameter should only be used
                 for unit testing.  Defaults to sys.
      target_version: A string representing the Python version to compare
                      the current version against.  The string should have
                      one of the following three forms: 2, 2.5, or 2.5.3.
                      Defaults to the minimum version that the webkitpy
                      package supports.

    Returns:
      A triple of (comparison, current_version, target_version).

      comparison: An integer representing the result of comparing the
                  current version with the target version.  A positive
                  number means the current version is greater than the
                  target, 0 means they are the same, and a negative number
                  means the current version is less than the target.
                      This method compares version information only up
                  to the precision of the given target version.  For
                  example, if the target version is 2.6 and the current
                  version is 2.5.3, this method uses 2.5 for the purposes
                  of comparing with the target.
      current_version: A string representing the current Python version, for
                       example 2.5.3.
      target_version: A string representing the version that the current
                      version was compared against, for example 2.5.

    """
    if sysmodule is None:
        sysmodule = sys
    if target_version is None:
        target_version = _MINIMUM_SUPPORTED_PYTHON_VERSION

    # The number of version parts to compare.
    precision = len(target_version.split("."))

    # We use sys.version_info rather than sys.version since its first
    # three elements are guaranteed to be integers.
    current_version_info_to_compare = sysmodule.version_info[:precision]
    # Convert integers to strings.
    current_version_info_to_compare = map(str, current_version_info_to_compare)
    current_version_to_compare = ".".join(current_version_info_to_compare)

    # Compare version strings lexicographically.
    if current_version_to_compare > target_version:
        comparison = 1
    elif current_version_to_compare == target_version:
        comparison = 0
    else:
        comparison = -1

    # The version number portion of the current version string, for
    # example "2.6.4".
    current_version = sysmodule.version.split()[0]

    return (comparison, current_version, target_version)


# FIXME: Add a logging level parameter to allow the version message
#        to be logged at levels other than WARNING, for example CRITICAL.
def check_version(log=None, sysmodule=None, target_version=None):
    """Check the current Python version against a target version.

    Logs a warning message if the current version is less than the
    target version.

    Args:
      log: A logging.logger instance to use when logging the version warning.
           Defaults to the logger of this module.
      sysmodule: See the compare_version() docstring.
      target_version: See the compare_version() docstring.

    Returns:
      A boolean value of whether the current version is greater than
      or equal to the target version.

    """
    if log is None:
        log = _log

    (comparison, current_version, target_version) = \
        compare_version(sysmodule, target_version)

    if comparison >= 0:
        # Then the current version is at least the minimum version.
        return True

    message = ("WebKit Python scripts do not support your current Python "
               "version (%s).  The minimum supported version is %s.\n"
               "  See the following page to upgrade your Python version:\n\n"
               "    http://trac.webkit.org/wiki/PythonGuidelines\n"
               % (current_version, target_version))
    log.warn(message)
    return False
