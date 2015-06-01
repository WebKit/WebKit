# Copyright (C) 2012 Intel Inc. All rights reserved.
# Copyright (C) 2013 Apple Inc. All rights reserved.
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


"""Supports checking WebKit style in cmake files.(.cmake, CMakeLists.txt)"""

from common import TabChecker, match, search, searchIgnorecase


class CMakeChecker(object):

    """Processes CMake lines for checking style."""

    # NO_SPACE_CMDS list are based on commands section of CMake document.
    # Now it is generated from
    # http://www.cmake.org/cmake/help/v2.8.10/cmake.html#section_Commands.
    # Some commands are from default CMake modules such as pkg_check_modules.
    # Please keep list in alphabet order.
    #
    # For commands in this list, spaces should not be added it and its
    # parentheses. For eg, message("testing"), not message ("testing")
    #
    # The conditional commands like if, else, endif, foreach, endforeach,
    # while, endwhile and break are listed in ONE_SPACE_CMDS
    NO_SPACE_CMDS = [
        'add_custom_command', 'add_custom_target', 'add_definitions',
        'add_dependencies', 'add_executable', 'add_library',
        'add_subdirectory', 'add_test', 'aux_source_directory',
        'build_command',
        'cmake_minimum_required', 'cmake_policy', 'configure_file',
        'create_test_sourcelist',
        'define_property',
        'enable_language', 'enable_testing', 'endfunction', 'endmacro',
        'execute_process', 'export',
        'file', 'find_file', 'find_library', 'find_package', 'find_path',
        'find_program', 'fltk_wrap_ui', 'function',
        'get_cmake_property', 'get_directory_property',
        'get_filename_component', 'get_property', 'get_source_file_property',
        'get_target_property', 'get_test_property',
        'include', 'include_directories', 'include_external_msproject',
        'include_regular_expression', 'install',
        'link_directories', 'list', 'load_cache', 'load_command',
        'macro', 'mark_as_advanced', 'math', 'message',
        'option',
        #From FindPkgConfig.cmake
        'pkg_check_modules',
        'project',
        'remove_definitions', 'return',
        'separate_arguments', 'set', 'set_directory_properties', 'set_property',
        'set_source_files_properties', 'set_target_properties',
        'set_tests_properties', 'site_name', 'source_group', 'string',
        'target_link_libraries', 'try_compile', 'try_run',
        'unset',
        'variable_watch',
    ]

    # CMake conditional commands, require one space between command and
    # its parentheses, such as "if (", "foreach (", etc.
    ONE_SPACE_CMDS = [
        'if', 'else', 'elseif', 'endif',
        'foreach', 'endforeach',
        'while', 'endwhile',
        'break',
    ]

    def __init__(self, file_path, handle_style_error):
        self._handle_style_error = handle_style_error
        self._tab_checker = TabChecker(file_path, handle_style_error)

    def check(self, lines):
        self._tab_checker.check(lines)
        for line_number, line in enumerate(lines, start=1):
            self._process_line(line_number, line)
        self._check_list_order(lines)

    def _process_line(self, line_number, line_content):
        if match('(^|\ +)#', line_content):
            # ignore comment line
            return
        l = line_content.expandtabs(4)
        # check command like message( "testing")
        if search('\(\ +', l):
            self._handle_style_error(line_number, 'whitespace/parentheses', 5,
                                     'No space after "("')
        # check command like message("testing" )
        if search('\ +\)', l) and not search('^\ +\)$', l):
            self._handle_style_error(line_number, 'whitespace/parentheses', 5,
                                     'No space before ")"')
        self._check_trailing_whitespace(line_number, l)
        self._check_no_space_cmds(line_number, l)
        self._check_one_space_cmds(line_number, l)
        self._check_indent(line_number, line_content)

    def _check_trailing_whitespace(self, line_number, line_content):
        line_content = line_content.rstrip('\n')    # chr(10), newline
        line_content = line_content.rstrip('\r')    # chr(13), carriage return
        line_content = line_content.rstrip('\x0c')  # chr(12), form feed, ^L
        stripped = line_content.rstrip()
        if line_content != stripped:
            self._handle_style_error(line_number, 'whitespace/trailing', 5,
                                     'No trailing spaces')

    def _check_no_space_cmds(self, line_number, line_content):
        # check command like "SET    (" or "Set("
        for t in self.NO_SPACE_CMDS:
            self._check_non_lowercase_cmd(line_number, line_content, t)
            if search('(^|\ +)' + t.lower() + '\ +\(', line_content):
                msg = 'No space between command "' + t.lower() + '" and its parentheses, should be "' + t + '("'
                self._handle_style_error(line_number, 'whitespace/parentheses', 5, msg)

    def _check_one_space_cmds(self, line_number, line_content):
        # check command like "IF (" or "if(" or "if   (" or "If ()"
        for t in self.ONE_SPACE_CMDS:
            self._check_non_lowercase_cmd(line_number, line_content, t)
            if search('(^|\ +)' + t.lower() + '(\(|\ \ +\()', line_content):
                msg = 'One space between command "' + t.lower() + '" and its parentheses, should be "' + t + ' ("'
                self._handle_style_error(line_number, 'whitespace/parentheses', 5, msg)

    def _check_non_lowercase_cmd(self, line_number, line_content, cmd):
        if searchIgnorecase('(^|\ +)' + cmd + '\ *\(', line_content) and \
           (not search('(^|\ +)' + cmd.lower() + '\ *\(', line_content)):
            msg = 'Use lowercase command "' + cmd.lower() + '"'
            self._handle_style_error(line_number, 'command/lowercase', 5, msg)

    def _check_indent(self, line_number, line_content):
        #TODO (halton): add indent checking
        pass

    def _check_list_order(self, lines):
        last_line = None

        for line_number, line in enumerate(lines, start=1):
            matched = search('\$\{.*\}', line)
            if matched:
                continue
            line = line.strip()

            if last_line == None:
                matched = match('(set\(|list\((APPEND|REMOVE_ITEM) )(?P<name>\w+)(?P<item>\s+\w+)?$', line)
                if matched:
                    # FIXME: Add handling for include directories.
                    if 'INCLUDE_DIRECTORIES' in matched.group('name'):
                        continue
                    empty_lines_count = 0
                    last_line = ''
                    if matched.group('item'):
                        msg = 'First listitem "%s" should be in a new line.' % matched.group('item').strip()
                        self._handle_style_error(line_number, 'list/parentheses', 5, msg)
            else:
                matched = match('(?P<item>.+)?\)$', line)
                if matched:
                    last_line = None
                    if matched.group('item'):
                        msg = 'The parentheses after the last listitem "%s" should be in a new line.' % matched.group('item').strip()
                        self._handle_style_error(line_number, 'list/parentheses', 5, msg)
                elif line == '':
                    empty_lines_count += 1
                else:
                    last_line_path = self._list_item_path(last_line)
                    line_path = self._list_item_path(line)

                    if line == last_line:
                        msg = 'The item "%s" should be added only once to the list.' % line
                        self._handle_style_error(line_number, 'list/duplicate', 5, msg)
                    elif line_path < last_line_path or line_path == last_line_path and line < last_line:
                        msg = 'Alphabetical sorting problem. "%s" should be before "%s".' % (line, last_line)
                        self._handle_style_error(line_number, 'list/order', 5, msg)
                    elif last_line != '':
                        if line_path != last_line_path:
                            if empty_lines_count != 1:
                                msg = 'There should be exactly one empty line instead of %d between "%s" and "%s".' % (empty_lines_count, last_line, line)
                                self._handle_style_error(line_number, 'list/emptyline', 5, msg)
                        elif empty_lines_count != 0:
                            msg = 'There should be no empty line between "%s" and "%s".' % (last_line, line)
                            self._handle_style_error(line_number, 'list/emptyline', 5, msg)

                    last_line = line
                    empty_lines_count = 0

    def _list_item_path(self, item):
        token = item.split('/')
        if len(token) < 2:
            return ''
        return '/'.join(token[:-1])
