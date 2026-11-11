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

import unittest

from unittest.mock import MagicMock

from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.style.checkers.swift_build_registration import _imports_swiftui, build_file_entries, SwiftBuildRegistrationChecker


CWD = '/mock-checkout'
TEST_DIRECTORY = CWD + '/Tools/TestWebKitAPI'
PROJECT_PATH = TEST_DIRECTORY + '/TestWebKitAPI.xcodeproj/project.pbxproj'
CMAKE_PATH = TEST_DIRECTORY + '/PlatformCocoa.cmake'

# Xcode names a source relative to the synchronized folder holding it, and quotes it only
# when it contains a space.
PROJECT_CONTENTS = '\n'.join([
    '\t\t07AA27602F83C3A900FE10FC /* Exceptions for "Tests" folder in "TestWebKitAPI" target */ = {',
    '\t\t\tisa = PBXFileSystemSynchronizedBuildFileExceptionSet;',
    '\t\t\tmembershipExceptions = (',
    '\t\t\t\t"WebKit/WebPage/Mac Touch Support Tests/BasicMacTouchSupportTests.swift",',
    '\t\t\t\tWebKit/WebPage/InBoth.swift,',
    '\t\t\t\tWebKit/WebPage/OnlyInXcode.swift,',
    '\t\t\t);',
    '\t\t\ttarget = 8DD76F960486AA7600D96B5E /* TestWebKitAPI */;',
    '\t\t};',
    '\t\t07C907B32F820907003A09D8 /* Exceptions for "Helpers" folder in "TestWebKitAPILibrary" target */ = {',
    '\t\t\tisa = PBXFileSystemSynchronizedBuildFileExceptionSet;',
    '\t\t\tmembershipExceptions = (',
    '\t\t\t\tcocoa/HTTPServer.swift,',
    '\t\t\t);',
    '\t\t\ttarget = 7CCE7E8B1A41144E00447C4C /* TestWebKitAPILibrary */;',
    '\t\t};',
])

CMAKE_CONTENTS = '\n'.join([
    'add_library(TestWebKitAPILibrary OBJECT',
    '    ${TESTWEBKITAPI_DIR}/Helpers/cocoa/HTTPServer.swift',
    '    ${TESTWEBKITAPI_DIR}/Helpers/cocoa/OnlyInCMake.swift',
    ')',
    '',
    'list(APPEND TestWebKit_SOURCES',
    '    Tests/WebKit/WebPage/InBoth.swift',
    ')',
    '',
    '# FIXME: Support tests which need the _WebKit_SwiftUI cross-import overlay linked.',
])


class SwiftBuildRegistrationCheckerTest(unittest.TestCase):

    def check(self, files, project_contents=PROJECT_CONTENTS, cmake_contents=CMAKE_CONTENTS,
              existing_files=None, source='import Testing\n'):
        """Runs the checker and returns the errors it reported as (file path, message)."""
        contents = {}
        if project_contents is not None:
            contents[PROJECT_PATH] = project_contents
        if cmake_contents is not None:
            contents[CMAKE_PATH] = cmake_contents
        # A file with modified lines is on disk; a file which was removed is not.
        for path, line_numbers in files.items():
            if line_numbers is not None:
                contents.setdefault(path, source)
        for path in existing_files or []:
            contents.setdefault(path, source)

        host = MagicMock()
        host.filesystem = MockFileSystem(contents)
        configuration = MagicMock()
        configuration.is_reportable.return_value = True
        increment_error_count = MagicMock()

        SwiftBuildRegistrationChecker.check_registrations(
            files, configuration, CWD, increment_error_count, host=host)

        self.assertEqual(increment_error_count.call_count, configuration.write_style_error.call_count)
        errors = []
        for call in configuration.write_style_error.call_args_list:
            self.assertEqual(call.kwargs['category'], 'swift/buildsystem')
            self.assertEqual(call.kwargs['confidence_in_error'], 5)
            # Reported at line 0 so the error survives whichever lines the patch touched.
            self.assertEqual(call.kwargs['line_number'], 0)
            errors.append((call.kwargs['file_path'], call.kwargs['message']))
        return errors

    @staticmethod
    def added(path, line_count=3):
        return {path: list(range(1, line_count + 1))}

    def test_added_to_both_builds(self):
        self.assertEqual(self.check(self.added(TEST_DIRECTORY + '/Tests/WebKit/WebPage/InBoth.swift')), [])

    def test_helper_added_to_both_builds(self):
        self.assertEqual(self.check(self.added(TEST_DIRECTORY + '/Helpers/cocoa/HTTPServer.swift')), [])

    def test_added_to_xcode_only(self):
        path = TEST_DIRECTORY + '/Tests/WebKit/WebPage/OnlyInXcode.swift'
        errors = self.check(self.added(path))
        self.assertEqual(len(errors), 1)
        self.assertEqual(errors[0][0], path)
        self.assertIn('not in Tools/TestWebKitAPI/PlatformCocoa.cmake', errors[0][1])
        self.assertIn('list(APPEND TestWebKit_SOURCES ...)', errors[0][1])

    def test_added_to_cmake_only(self):
        path = TEST_DIRECTORY + '/Helpers/cocoa/OnlyInCMake.swift'
        errors = self.check(self.added(path))
        self.assertEqual(len(errors), 1)
        self.assertEqual(errors[0][0], path)
        self.assertIn('is not named in', errors[0][1])
        self.assertIn('TestWebKitAPILibrary', errors[0][1])

    def test_added_to_neither_build(self):
        errors = self.check(self.added(TEST_DIRECTORY + '/Tests/WebKit/WebPage/Nowhere.swift'))
        self.assertEqual(len(errors), 1)
        self.assertIn('is in neither', errors[0][1])

    def test_path_with_spaces_is_quoted_in_the_xcode_project(self):
        path = TEST_DIRECTORY + '/Tests/WebKit/WebPage/Mac Touch Support Tests/BasicMacTouchSupportTests.swift'
        errors = self.check(self.added(path))
        # Named in the project file, so only the CMake half is reported.
        self.assertEqual(len(errors), 1)
        self.assertIn('not in Tools/TestWebKitAPI/PlatformCocoa.cmake', errors[0][1])

    def test_swiftui_test_is_not_required_in_cmake(self):
        # SwiftUI pulls in the _WebKit_SwiftUI overlay, which CMake cannot link yet.
        path = TEST_DIRECTORY + '/Tests/WebKit/WebPage/Mac Touch Support Tests/BasicMacTouchSupportTests.swift'
        self.assertEqual(self.check(
            self.added(path), source='import SwiftUI\nimport Testing\n'), [])

    def test_swiftui_test_is_still_required_in_the_xcode_project(self):
        path = TEST_DIRECTORY + '/Tests/WebKit/WebPage/NotInXcode.swift'
        errors = self.check(self.added(path), source='private import SwiftUI\n')
        self.assertEqual(len(errors), 1)
        self.assertIn('is not named in', errors[0][1])

    def test_swiftui_named_in_a_comment_is_not_an_import(self):
        path = TEST_DIRECTORY + '/Tests/WebKit/WebPage/OnlyInXcode.swift'
        errors = self.check(self.added(path), source='// This does not import SwiftUI\n')
        self.assertEqual(len(errors), 1)
        self.assertIn('not in Tools/TestWebKitAPI/PlatformCocoa.cmake', errors[0][1])

    def test_similar_file_name_is_not_a_match(self):
        # OnlyInXcode.swift is registered; MyOnlyInXcode.swift must not match it.
        errors = self.check(self.added(TEST_DIRECTORY + '/Tests/WebKit/WebPage/MyOnlyInXcode.swift'))
        self.assertEqual(len(errors), 1)
        self.assertIn('is in neither', errors[0][1])

    def test_modified_file_is_not_checked(self):
        self.assertEqual(self.check({TEST_DIRECTORY + '/Tests/WebKit/WebPage/OnlyInXcode.swift': [7, 8]}), [])

    def test_whole_file_check_is_not_an_addition(self):
        path = TEST_DIRECTORY + '/Tests/WebKit/WebPage/OnlyInXcode.swift'
        self.assertEqual(self.check({path: None}, existing_files=[path]), [])

    def test_folders_without_a_cmake_counterpart_are_out_of_scope(self):
        # Xcode synchronizes Runner and App wholesale, so their sources are named in
        # neither the project file nor a membershipExceptions list.
        self.assertEqual(self.check({
            TEST_DIRECTORY + '/Runner/TestRunner.swift': [1, 2, 3],
            TEST_DIRECTORY + '/App/mac/Scratch.swift': [1, 2, 3],
        }), [])

    def test_non_swift_and_non_test_files_are_ignored(self):
        self.assertEqual(self.check({
            TEST_DIRECTORY + '/Tests/WebKit/WebPage/Something.mm': [1, 2, 3],
            CWD + '/Source/WebKit/Elsewhere.swift': [1, 2, 3],
        }), [])

    def test_removed_file_still_in_both_builds(self):
        errors = self.check({TEST_DIRECTORY + '/Tests/WebKit/WebPage/InBoth.swift': None})
        self.assertEqual([error[0] for error in errors], [PROJECT_PATH, CMAKE_PATH])
        self.assertIn('still named in Tools/TestWebKitAPI/TestWebKitAPI.xcodeproj', errors[0][1])
        self.assertIn('still named in Tools/TestWebKitAPI/PlatformCocoa.cmake', errors[1][1])

    def test_removed_file_absent_from_both_builds(self):
        self.assertEqual(self.check({TEST_DIRECTORY + '/Tests/WebKit/WebPage/LongGone.swift': None}), [])

    def test_missing_project_file_reports_nothing(self):
        self.assertEqual(self.check(
            self.added(TEST_DIRECTORY + '/Tests/WebKit/WebPage/OnlyInXcode.swift'),
            project_contents=None), [])

    def test_project_file_without_swift_sources_reports_nothing(self):
        self.assertEqual(self.check(
            self.added(TEST_DIRECTORY + '/Tests/WebKit/WebPage/OnlyInXcode.swift'),
            project_contents='this is not a project file'), [])

    def test_missing_cmake_file_reports_nothing(self):
        self.assertEqual(self.check(
            self.added(TEST_DIRECTORY + '/Tests/WebKit/WebPage/OnlyInXcode.swift'),
            cmake_contents=None), [])

    def test_cmake_file_without_swift_sources_reports_nothing(self):
        self.assertEqual(self.check(
            self.added(TEST_DIRECTORY + '/Tests/WebKit/WebPage/OnlyInXcode.swift'),
            cmake_contents='add_library(TestWebKitAPILibrary OBJECT)\n'), [])

    def test_filtered_out_category_reports_nothing(self):
        files = self.added(TEST_DIRECTORY + '/Tests/WebKit/WebPage/OnlyInXcode.swift')
        host = MagicMock()
        host.filesystem = MockFileSystem({
            PROJECT_PATH: PROJECT_CONTENTS,
            CMAKE_PATH: CMAKE_CONTENTS,
            list(files)[0]: 'import Testing\n',
        })
        configuration = MagicMock()
        configuration.is_reportable.return_value = False
        increment_error_count = MagicMock()

        SwiftBuildRegistrationChecker.check_registrations(
            files, configuration, CWD, increment_error_count, host=host)

        self.assertEqual(configuration.write_style_error.call_count, 0)
        self.assertEqual(increment_error_count.call_count, 0)


class SwiftBuildRegistrationCheckedInFilesTest(unittest.TestCase):
    """Checks the real build files, so that reformatting either one cannot go unnoticed.

    Deliberately asserts nothing about individual files, so adding a Swift test never has
    to touch this test.
    """

    def setUp(self):
        self.filesystem = FileSystem()
        self.test_directory = WebKitFinder(self.filesystem).path_from_webkit_base('Tools', 'TestWebKitAPI')

    def entries(self, *components):
        entries = build_file_entries(self.filesystem, self.filesystem.join(self.test_directory, *components))
        self.assertIsNotNone(entries)
        return entries

    def test_checked_in_swift_files_are_in_both_builds(self):
        project_entries = self.entries('TestWebKitAPI.xcodeproj', 'project.pbxproj')
        cmake_entries = self.entries('PlatformCocoa.cmake')

        def is_swift(filesystem, dirname, basename):
            return basename.endswith('.swift')

        missing_from_xcode = []
        missing_from_cmake = []
        for path in self.filesystem.files_under(self.test_directory, file_filter=is_swift):
            relative_path = self.filesystem.relpath(path, self.test_directory).replace(self.filesystem.sep, '/')
            # Mirrors the folders the checker looks at.
            if not relative_path.startswith(('Tests/', 'Helpers/', 'TestWebKitAPILibrary/', 'TestWTFLibrary/')):
                continue
            if relative_path.split('/', 1)[1] not in project_entries:
                missing_from_xcode.append(relative_path)
            if relative_path not in cmake_entries and not _imports_swiftui(self.filesystem, path):
                missing_from_cmake.append(relative_path)

        self.assertEqual(missing_from_xcode, [])
        self.assertEqual(missing_from_cmake, [])
