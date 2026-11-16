# Copyright (C) 2026 Apple Inc. All rights reserved.
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

"""Checks that Swift test sources are registered in both the Xcode and the CMake build.
"""

import logging

from webkitpy.common.host import Host
from webkitpy.style.error_handlers import DefaultStyleErrorHandler


_log = logging.getLogger(__name__)

_CATEGORY = 'swift/buildsystem'

_TEST_DIRECTORY = 'Tools/TestWebKitAPI'
_PROJECT_FILE = _TEST_DIRECTORY + '/TestWebKitAPI.xcodeproj/project.pbxproj'
_CMAKE_FILE = _TEST_DIRECTORY + '/PlatformCocoa.cmake'

_CMAKE_DIRECTORY_VARIABLE = '${TESTWEBKITAPI_DIR}/'

# Importing SwiftUI alongside WebKit pulls in the _WebKit_SwiftUI cross-import overlay,
# which the CMake build cannot link yet.
_SWIFTUI_MODULES = ('SwiftUI', '_WebKit_SwiftUI')

# The folders holding Swift tests and their helpers, with the Xcode target a new source
# there normally belongs to and where it belongs in PlatformCocoa.cmake.
_DESTINATIONS = (
    ('Tests/WTF/', 'TestWTF', 'the list(APPEND TestWTF_SOURCES ...) block'),
    ('Tests/WebKit/', 'TestWebKitAPI', 'the list(APPEND TestWebKit_SOURCES ...) block'),
    ('Helpers/', 'TestWebKitAPILibrary', 'the add_library(TestWebKitAPILibrary OBJECT ...) source list'),
    ('TestWebKitAPILibrary/', 'TestWebKitAPILibrary', 'the add_library(TestWebKitAPILibrary OBJECT ...) source list'),
    ('TestWTFLibrary/', 'TestWTFLibrary', 'the add_library(TestWTFLibrary OBJECT ...) source list'),
)


def build_file_entries(filesystem, path):
    """Returns the set of paths a build file names, or None.

    None means the file could not be read, or names no Swift source at all, which can
    only mean its format changed.
    """
    try:
        contents = filesystem.read_text_file(path)
    except (IOError, OSError, UnicodeDecodeError) as error:
        _log.debug("%s: cannot read '%s': %s" % (_CATEGORY, path, error))
        return None

    entries = set()
    for line in contents.splitlines():
        # A path is quoted only when it contains a space, and is followed by a comma in
        # the Xcode project.
        entry = line.strip().rstrip(',').strip('"')
        if entry.startswith(_CMAKE_DIRECTORY_VARIABLE):
            entry = entry[len(_CMAKE_DIRECTORY_VARIABLE):]
        entries.add(entry)

    if not any(entry.endswith('.swift') for entry in entries):
        _log.debug("%s: no Swift sources named in '%s', skipping." % (_CATEGORY, path))
        return None

    return entries


def _imports_swiftui(filesystem, absolute_path):
    """Returns whether a Swift source imports SwiftUI, in any of the forms Swift allows."""
    try:
        contents = filesystem.read_text_file(absolute_path)
    except (IOError, OSError, UnicodeDecodeError):
        return False

    for line in contents.splitlines():
        words = line.split()
        # Covers 'import SwiftUI', 'private import SwiftUI' and 'import struct SwiftUI.X'.
        if len(words) < 2 or 'import' not in words or words[0].startswith('/'):
            continue
        if words[-1].split('.')[0] in _SWIFTUI_MODULES:
            return True
    return False


def _destination(relative_path):
    """Returns the Xcode target and the CMake source list for a path, or None."""
    for prefix, target, source_list in _DESTINATIONS:
        if relative_path.startswith(prefix):
            return target, source_list
    return None


def _project_entry(relative_path):
    """Returns how the Xcode project names a path.

    Xcode names a source relative to the synchronized folder holding it, so
    Tests/WebKit/WKWebView/CodingTests.swift appears as WebKit/WKWebView/CodingTests.swift.
    """
    return relative_path.split('/', 1)[1]


def _is_newly_added(line_numbers, absolute_path, filesystem):
    """Returns whether the patch adds the file, rather than modifying or removing it."""
    # None means the file was removed, or that a whole file was checked outside of a patch.
    if not line_numbers:
        return False
    if not filesystem.exists(absolute_path):
        return False
    # Every line of a newly added file is part of the patch, so the modified line numbers
    # of such a file are exactly 1 through N.
    return line_numbers == list(range(1, len(line_numbers) + 1))


class SwiftBuildRegistrationChecker(object):
    """Flags Swift test sources which reach only one of the two builds."""

    categories = set([_CATEGORY])

    @staticmethod
    def check_registrations(files, configuration, cwd, increment_error_count=lambda: 0, host=None):
        """Report Swift sources which are in the Xcode build or the CMake build but not both.

        Args:
            files: A dictionary mapping absolute file paths to the lists of lines
                modified in each file. Removed files map to None.
            configuration: A StyleProcessorConfiguration instance.
            cwd: The root of the SCM checkout, used to relativize the file paths.
            increment_error_count: Callable invoked once per reported error.
            host: The current host (for testing).
        """
        host = host or Host()
        filesystem = host.filesystem

        test_directory = filesystem.join(cwd, *_TEST_DIRECTORY.split('/'))
        project_path = filesystem.join(cwd, *_PROJECT_FILE.split('/'))
        cmake_path = filesystem.join(cwd, *_CMAKE_FILE.split('/'))

        added = []
        removed = []
        for absolute_path, line_numbers in files.items():
            if not absolute_path.endswith('.swift'):
                continue
            if not absolute_path.startswith(test_directory + filesystem.sep):
                continue
            relative_path = filesystem.relpath(absolute_path, test_directory).replace(filesystem.sep, '/')
            if not _destination(relative_path):
                continue
            if _is_newly_added(line_numbers, absolute_path, filesystem):
                added.append((absolute_path, relative_path))
            elif line_numbers is None and not filesystem.exists(absolute_path):
                removed.append(relative_path)

        # Both build files are only read when the patch can possibly be at fault.
        if not added and not removed:
            return

        project_entries = build_file_entries(filesystem, project_path)
        cmake_entries = build_file_entries(filesystem, cmake_path)
        if project_entries is None or cmake_entries is None:
            return

        def report(absolute_path, message):
            style_error_handler = DefaultStyleErrorHandler(
                absolute_path, configuration, increment_error_count, files.get(absolute_path))
            # Report at line 0 so the error is emitted regardless of which lines of the
            # file the patch happened to touch.
            style_error_handler(0, _CATEGORY, 5, message)

        for absolute_path, relative_path in sorted(added, key=lambda entry: entry[1]):
            target, source_list = _destination(relative_path)
            in_xcode = _project_entry(relative_path) in project_entries
            in_cmake = relative_path in cmake_entries or _imports_swiftui(filesystem, absolute_path)

            if not in_xcode and not in_cmake:
                report(absolute_path, (
                    '{path} is in neither {project} nor {cmake}, so no build will compile it. '
                    'Add it to both.').format(
                        path=relative_path, project=_PROJECT_FILE, cmake=_CMAKE_FILE))
            elif not in_xcode:
                report(absolute_path, (
                    '{path} is not named in {project}, so Xcode will not compile it. Select '
                    'the file in Xcode and check {target} under Target Membership in the File '
                    'Inspector.').format(
                        path=relative_path, project=_PROJECT_FILE, target=target))
            elif not in_cmake:
                report(absolute_path, (
                    '{path} is in the Xcode project but not in {cmake}, so the CMake build '
                    'will not compile it. Add it to {source_list}.').format(
                        path=relative_path, cmake=_CMAKE_FILE, source_list=source_list))

        for relative_path in sorted(removed):
            if _project_entry(relative_path) in project_entries:
                report(project_path, (
                    '{path} was removed but is still named in {project}. Remove it there '
                    'too.').format(path=relative_path, project=_PROJECT_FILE))
            if relative_path in cmake_entries:
                report(cmake_path, (
                    '{path} was removed but is still named in {cmake}. Remove it there '
                    'too.').format(path=relative_path, cmake=_CMAKE_FILE))
