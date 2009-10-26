#!/usr/bin/python
# -*- coding: utf-8; -*-
#
# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2009 Torch Mobile Inc.
# Copyright (C) 2009 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
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

"""Unit test for cpp_style.py."""

# FIXME: Add a good test that tests UpdateIncludeState.

import codecs
import os
import random
import re
import unittest
import cpp_style


# This class works as an error collector and replaces cpp_style.Error
# function for the unit tests.  We also verify each category we see
# is in cpp_style._ERROR_CATEGORIES, to help keep that list up to date.
class ErrorCollector:
    # These are a global list, covering all categories seen ever.
    _ERROR_CATEGORIES = [x.strip()    # get rid of leading whitespace
                         for x in cpp_style._ERROR_CATEGORIES.split()]
    _SEEN_ERROR_CATEGORIES = {}

    def __init__(self, assert_fn):
        """assert_fn: a function to call when we notice a problem."""
        self._assert_fn = assert_fn
        self._errors = []

    def __call__(self, unused_filename, unused_linenum,
                 category, confidence, message):
        self._assert_fn(category in self._ERROR_CATEGORIES,
                        'Message "%s" has category "%s",'
                        ' which is not in _ERROR_CATEGORIES' % (message, category))
        self._SEEN_ERROR_CATEGORIES[category] = 1
        if cpp_style._should_print_error(category, confidence):
            self._errors.append('%s  [%s] [%d]' % (message, category, confidence))

    def results(self):
        if len(self._errors) < 2:
            return ''.join(self._errors)  # Most tests expect to have a string.
        else:
            return self._errors  # Let's give a list if there is more than one.

    def result_list(self):
        return self._errors

    def verify_all_categories_are_seen(self):
        """Fails if there's a category in _ERROR_CATEGORIES - _SEEN_ERROR_CATEGORIES.

        This should only be called after all tests are run, so
        _SEEN_ERROR_CATEGORIES has had a chance to fully populate.  Since
        this isn't called from within the normal unittest framework, we
        can't use the normal unittest assert macros.  Instead we just exit
        when we see an error.  Good thing this test is always run last!
        """
        for category in self._ERROR_CATEGORIES:
            if category not in self._SEEN_ERROR_CATEGORIES:
                import sys
                sys.exit('FATAL ERROR: There are no tests for category "%s"' % category)

    def remove_if_present(self, substr):
        for (index, error) in enumerate(self._errors):
            if error.find(substr) != -1:
                self._errors = self._errors[0:index] + self._errors[(index + 1):]
                break


# This class is a lame mock of codecs. We do not verify filename, mode, or
# encoding, but for the current use case it is not needed.
class MockIo:
    def __init__(self, mock_file):
        self.mock_file = mock_file

    def open(self, unused_filename, unused_mode, unused_encoding, _):  # NOLINT
        # (lint doesn't like open as a method name)
        return self.mock_file


class CppStyleTestBase(unittest.TestCase):
    """Provides some useful helper functions for cpp_style tests."""

    # Perform lint on single line of input and return the error message.
    def perform_single_line_lint(self, code, file_name):
        error_collector = ErrorCollector(self.assert_)
        lines = code.split('\n')
        cpp_style.remove_multi_line_comments(file_name, lines, error_collector)
        clean_lines = cpp_style.CleansedLines(lines)
        include_state = cpp_style._IncludeState()
        function_state = cpp_style._FunctionState()
        ext = file_name[file_name.rfind('.') + 1:]
        class_state = cpp_style._ClassState()
        cpp_style.process_line(file_name, ext, clean_lines, 0,
                               include_state, function_state,
                               class_state, error_collector)
        # Single-line lint tests are allowed to fail the 'unlintable function'
        # check.
        error_collector.remove_if_present(
            'Lint failed to find start of function body.')
        return error_collector.results()

    # Perform lint over multiple lines and return the error message.
    def perform_multi_line_lint(self, code, file_name):
        error_collector = ErrorCollector(self.assert_)
        lines = code.split('\n')
        cpp_style.remove_multi_line_comments(file_name, lines, error_collector)
        lines = cpp_style.CleansedLines(lines)
        ext = file_name[file_name.rfind('.') + 1:]
        class_state = cpp_style._ClassState()
        for i in xrange(lines.num_lines()):
            cpp_style.check_style(file_name, lines, i, ext, error_collector)
            cpp_style.check_for_non_standard_constructs(file_name, lines, i, class_state,
                                                        error_collector)
        class_state.check_finished(file_name, error_collector)
        return error_collector.results()

    # Similar to perform_multi_line_lint, but calls check_language instead of
    # check_for_non_standard_constructs
    def perform_language_rules_check(self, file_name, code):
        error_collector = ErrorCollector(self.assert_)
        include_state = cpp_style._IncludeState()
        lines = code.split('\n')
        cpp_style.remove_multi_line_comments(file_name, lines, error_collector)
        lines = cpp_style.CleansedLines(lines)
        ext = file_name[file_name.rfind('.') + 1:]
        for i in xrange(lines.num_lines()):
            cpp_style.check_language(file_name, lines, i, ext, include_state,
                                     error_collector)
        return error_collector.results()

    def perform_function_lengths_check(self, code):
        """Perform Lint function length check on block of code and return warnings.

        Builds up an array of lines corresponding to the code and strips comments
        using cpp_style functions.

        Establishes an error collector and invokes the function length checking
        function following cpp_style's pattern.

        Args:
          code: C++ source code expected to generate a warning message.

        Returns:
          The accumulated errors.
        """
        file_name = 'foo.cpp'
        error_collector = ErrorCollector(self.assert_)
        function_state = cpp_style._FunctionState()
        lines = code.split('\n')
        cpp_style.remove_multi_line_comments(file_name, lines, error_collector)
        lines = cpp_style.CleansedLines(lines)
        for i in xrange(lines.num_lines()):
            cpp_style.check_for_function_lengths(file_name, lines, i,
                                                 function_state, error_collector)
        return error_collector.results()

    def perform_include_what_you_use(self, code, filename='foo.h', io=codecs):
        # First, build up the include state.
        error_collector = ErrorCollector(self.assert_)
        include_state = cpp_style._IncludeState()
        lines = code.split('\n')
        cpp_style.remove_multi_line_comments(filename, lines, error_collector)
        lines = cpp_style.CleansedLines(lines)
        for i in xrange(lines.num_lines()):
            cpp_style.check_language(filename, lines, i, '.h', include_state,
                                     error_collector)
        # We could clear the error_collector here, but this should
        # also be fine, since our IncludeWhatYouUse unittests do not
        # have language problems.

        # Second, look for missing includes.
        cpp_style.check_for_include_what_you_use(filename, lines, include_state,
                                                 error_collector, io)
        return error_collector.results()

    # Perform lint and compare the error message with "expected_message".
    def assert_lint(self, code, expected_message, file_name='foo.cpp'):
        self.assertEquals(expected_message, self.perform_single_line_lint(code, file_name))

    def assert_lint_one_of_many_errors_re(self, code, expected_message_re, file_name='foo.cpp'):
        messages = self.perform_single_line_lint(code, file_name)
        for message in messages:
            if re.search(expected_message_re, message):
                return

        self.assertEquals(expected_message, messages)

    def assert_multi_line_lint(self, code, expected_message, file_name='foo.h'):
        self.assertEquals(expected_message, self.perform_multi_line_lint(code, file_name))

    def assert_multi_line_lint_re(self, code, expected_message_re, file_name='foo.h'):
        message = self.perform_multi_line_lint(code, file_name)
        if not re.search(expected_message_re, message):
            self.fail('Message was:\n' + message + 'Expected match to "' + expected_message_re + '"')

    def assert_language_rules_check(self, file_name, code, expected_message):
        self.assertEquals(expected_message,
                          self.perform_language_rules_check(file_name, code))

    def assert_include_what_you_use(self, code, expected_message):
        self.assertEquals(expected_message,
                          self.perform_include_what_you_use(code))

    def assert_blank_lines_check(self, lines, start_errors, end_errors):
        error_collector = ErrorCollector(self.assert_)
        cpp_style.process_file_data('foo.cpp', 'cpp', lines, error_collector)
        self.assertEquals(
            start_errors,
            error_collector.results().count(
                'Blank line at the start of a code block.  Is this needed?'
                '  [whitespace/blank_line] [2]'))
        self.assertEquals(
            end_errors,
            error_collector.results().count(
                'Blank line at the end of a code block.  Is this needed?'
                '  [whitespace/blank_line] [3]'))


class CppStyleTest(CppStyleTestBase):

    # Test get line width.
    def test_get_line_width(self):
        self.assertEquals(0, cpp_style.get_line_width(''))
        self.assertEquals(10, cpp_style.get_line_width(u'x' * 10))
        self.assertEquals(16, cpp_style.get_line_width(u'都|道|府|県|支庁'))

    def test_find_next_multi_line_comment_start(self):
        self.assertEquals(1, cpp_style.find_next_multi_line_comment_start([''], 0))

        lines = ['a', 'b', '/* c']
        self.assertEquals(2, cpp_style.find_next_multi_line_comment_start(lines, 0))

        lines = ['char a[] = "/*";']  # not recognized as comment.
        self.assertEquals(1, cpp_style.find_next_multi_line_comment_start(lines, 0))

    def test_find_next_multi_line_comment_end(self):
        self.assertEquals(1, cpp_style.find_next_multi_line_comment_end([''], 0))
        lines = ['a', 'b', ' c */']
        self.assertEquals(2, cpp_style.find_next_multi_line_comment_end(lines, 0))

    def test_remove_multi_line_comments_from_range(self):
        lines = ['a', '  /* comment ', ' * still comment', ' comment */   ', 'b']
        cpp_style.remove_multi_line_comments_from_range(lines, 1, 4)
        self.assertEquals(['a', '// dummy', '// dummy', '// dummy', 'b'], lines)

    def test_spaces_at_end_of_line(self):
        self.assert_lint(
            '// Hello there ',
            'Line ends in whitespace.  Consider deleting these extra spaces.'
            '  [whitespace/end_of_line] [4]')

    # Test C-style cast cases.
    def test_cstyle_cast(self):
        self.assert_lint(
            'int a = (int)1.0;',
            'Using C-style cast.  Use static_cast<int>(...) instead'
            '  [readability/casting] [4]')
        self.assert_lint(
            'int *a = (int *)DEFINED_VALUE;',
            'Using C-style cast.  Use reinterpret_cast<int *>(...) instead'
            '  [readability/casting] [4]', 'foo.c')
        self.assert_lint(
            'uint16 a = (uint16)1.0;',
            'Using C-style cast.  Use static_cast<uint16>(...) instead'
            '  [readability/casting] [4]')
        self.assert_lint(
            'int32 a = (int32)1.0;',
            'Using C-style cast.  Use static_cast<int32>(...) instead'
            '  [readability/casting] [4]')
        self.assert_lint(
            'uint64 a = (uint64)1.0;',
            'Using C-style cast.  Use static_cast<uint64>(...) instead'
            '  [readability/casting] [4]')

    # Test taking address of casts (runtime/casting)
    def test_runtime_casting(self):
        self.assert_lint(
            'int* x = &static_cast<int*>(foo);',
            'Are you taking an address of a cast?  '
            'This is dangerous: could be a temp var.  '
            'Take the address before doing the cast, rather than after'
            '  [runtime/casting] [4]')

        self.assert_lint(
            'int* x = &dynamic_cast<int *>(foo);',
            ['Are you taking an address of a cast?  '
             'This is dangerous: could be a temp var.  '
             'Take the address before doing the cast, rather than after'
             '  [runtime/casting] [4]',
             'Do not use dynamic_cast<>.  If you need to cast within a class '
             'hierarchy, use static_cast<> to upcast.  Google doesn\'t support '
             'RTTI.  [runtime/rtti] [5]'])

        self.assert_lint(
            'int* x = &reinterpret_cast<int *>(foo);',
            'Are you taking an address of a cast?  '
            'This is dangerous: could be a temp var.  '
            'Take the address before doing the cast, rather than after'
            '  [runtime/casting] [4]')

        # It's OK to cast an address.
        self.assert_lint(
            'int* x = reinterpret_cast<int *>(&foo);',
            '')

    def test_runtime_selfinit(self):
        self.assert_lint(
            'Foo::Foo(Bar r, Bel l) : r_(r_), l_(l_) { }',
            'You seem to be initializing a member variable with itself.'
            '  [runtime/init] [4]')
        self.assert_lint(
            'Foo::Foo(Bar r, Bel l) : r_(r), l_(l) { }',
            '')
        self.assert_lint(
            'Foo::Foo(Bar r) : r_(r), l_(r_), ll_(l_) { }',
            '')

    def test_runtime_rtti(self):
        statement = 'int* x = dynamic_cast<int*>(&foo);'
        error_message = (
            'Do not use dynamic_cast<>.  If you need to cast within a class '
            'hierarchy, use static_cast<> to upcast.  Google doesn\'t support '
            'RTTI.  [runtime/rtti] [5]')
        # dynamic_cast is disallowed in most files.
        self.assert_language_rules_check('foo.cpp', statement, error_message)
        self.assert_language_rules_check('foo.h', statement, error_message)
        # It is explicitly allowed in tests, however.
        self.assert_language_rules_check('foo_test.cpp', statement, '')
        self.assert_language_rules_check('foo_unittest.cpp', statement, '')
        self.assert_language_rules_check('foo_regtest.cpp', statement, '')

    # We cannot test this functionality because of difference of
    # function definitions.  Anyway, we may never enable this.
    #
    # # Test for unnamed arguments in a method.
    # def test_check_for_unnamed_params(self):
    #   message = ('All parameters should be named in a function'
    #              '  [readability/function] [3]')
    #   self.assert_lint('virtual void A(int*) const;', message)
    #   self.assert_lint('virtual void B(void (*fn)(int*));', message)
    #   self.assert_lint('virtual void C(int*);', message)
    #   self.assert_lint('void *(*f)(void *) = x;', message)
    #   self.assert_lint('void Method(char*) {', message)
    #   self.assert_lint('void Method(char*);', message)
    #   self.assert_lint('void Method(char* /*x*/);', message)
    #   self.assert_lint('typedef void (*Method)(int32);', message)
    #   self.assert_lint('static void operator delete[](void*) throw();', message)
    # 
    #   self.assert_lint('virtual void D(int* p);', '')
    #   self.assert_lint('void operator delete(void* x) throw();', '')
    #   self.assert_lint('void Method(char* x)\n{', '')
    #   self.assert_lint('void Method(char* /*x*/)\n{', '')
    #   self.assert_lint('void Method(char* x);', '')
    #   self.assert_lint('typedef void (*Method)(int32 x);', '')
    #   self.assert_lint('static void operator delete[](void* x) throw();', '')
    #   self.assert_lint('static void operator delete[](void* /*x*/) throw();', '')
    # 
    #   # This one should technically warn, but doesn't because the function
    #   # pointer is confusing.
    #   self.assert_lint('virtual void E(void (*fn)(int* p));', '')

    # Test deprecated casts such as int(d)
    def test_deprecated_cast(self):
        self.assert_lint(
            'int a = int(2.2);',
            'Using deprecated casting style.  '
            'Use static_cast<int>(...) instead'
            '  [readability/casting] [4]')
        # Checks for false positives...
        self.assert_lint(
            'int a = int();  // Constructor, o.k.',
            '')
        self.assert_lint(
            'X::X() : a(int()) {}  // default Constructor, o.k.',
            '')
        self.assert_lint(
            'operator bool();  // Conversion operator, o.k.',
            '')

    # The second parameter to a gMock method definition is a function signature
    # that often looks like a bad cast but should not picked up by lint.
    def test_mock_method(self):
        self.assert_lint(
            'MOCK_METHOD0(method, int());',
            '')
        self.assert_lint(
            'MOCK_CONST_METHOD1(method, float(string));',
            '')
        self.assert_lint(
            'MOCK_CONST_METHOD2_T(method, double(float, float));',
            '')

    # Test sizeof(type) cases.
    def test_sizeof_type(self):
        self.assert_lint(
            'sizeof(int);',
            'Using sizeof(type).  Use sizeof(varname) instead if possible'
            '  [runtime/sizeof] [1]')
        self.assert_lint(
            'sizeof(int *);',
            'Using sizeof(type).  Use sizeof(varname) instead if possible'
            '  [runtime/sizeof] [1]')

    # Test typedef cases.  There was a bug that cpp_style misidentified
    # typedef for pointer to function as C-style cast and produced
    # false-positive error messages.
    def test_typedef_for_pointer_to_function(self):
        self.assert_lint(
            'typedef void (*Func)(int x);',
            '')
        self.assert_lint(
            'typedef void (*Func)(int *x);',
            '')
        self.assert_lint(
            'typedef void Func(int x);',
            '')
        self.assert_lint(
            'typedef void Func(int *x);',
            '')

    def test_include_what_you_use_no_implementation_files(self):
        code = 'std::vector<int> foo;'
        self.assertEquals('Add #include <vector> for vector<>'
                          '  [build/include_what_you_use] [4]',
                          self.perform_include_what_you_use(code, 'foo.h'))
        self.assertEquals('',
                          self.perform_include_what_you_use(code, 'foo.cpp'))

    def test_include_what_you_use(self):
        self.assert_include_what_you_use(
            '''#include <vector>
               std::vector<int> foo;
            ''',
            '')
        self.assert_include_what_you_use(
            '''#include <map>
               std::pair<int,int> foo;
            ''',
            '')
        self.assert_include_what_you_use(
            '''#include <multimap>
               std::pair<int,int> foo;
            ''',
            '')
        self.assert_include_what_you_use(
            '''#include <hash_map>
               std::pair<int,int> foo;
            ''',
            '')
        self.assert_include_what_you_use(
            '''#include <utility>
               std::pair<int,int> foo;
            ''',
            '')
        self.assert_include_what_you_use(
            '''#include <vector>
               DECLARE_string(foobar);
            ''',
            '')
        self.assert_include_what_you_use(
            '''#include <vector>
               DEFINE_string(foobar, "", "");
            ''',
            '')
        self.assert_include_what_you_use(
            '''#include <vector>
               std::pair<int,int> foo;
            ''',
            'Add #include <utility> for pair<>'
            '  [build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            '''#include "base/foobar.h"
               std::vector<int> foo;
            ''',
            'Add #include <vector> for vector<>'
            '  [build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            '''#include <vector>
               std::set<int> foo;
            ''',
            'Add #include <set> for set<>'
            '  [build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            '''#include "base/foobar.h"
              hash_map<int, int> foobar;
            ''',
            'Add #include <hash_map> for hash_map<>'
            '  [build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            '''#include "base/foobar.h"
               bool foobar = std::less<int>(0,1);
            ''',
            'Add #include <functional> for less<>'
            '  [build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            '''#include "base/foobar.h"
               bool foobar = min<int>(0,1);
            ''',
            'Add #include <algorithm> for min  [build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            'void a(const string &foobar);',
            'Add #include <string> for string  [build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            '''#include "base/foobar.h"
               bool foobar = swap(0,1);
            ''',
            'Add #include <algorithm> for swap  [build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            '''#include "base/foobar.h"
               bool foobar = transform(a.begin(), a.end(), b.start(), Foo);
            ''',
            'Add #include <algorithm> for transform  '
            '[build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            '''#include "base/foobar.h"
               bool foobar = min_element(a.begin(), a.end());
            ''',
            'Add #include <algorithm> for min_element  '
            '[build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            '''foo->swap(0,1);
               foo.swap(0,1);
            ''',
            '')
        self.assert_include_what_you_use(
            '''#include <string>
               void a(const std::multimap<int,string> &foobar);
            ''',
            'Add #include <map> for multimap<>'
            '  [build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            '''#include <queue>
               void a(const std::priority_queue<int> &foobar);
            ''',
            '')
        self.assert_include_what_you_use(
             '''#include "base/basictypes.h"
                #include "base/port.h"
                #include <assert.h>
                #include <string>
                #include <vector>
                vector<string> hajoa;''', '')
        self.assert_include_what_you_use(
            '''#include <string>
               int i = numeric_limits<int>::max()
            ''',
            'Add #include <limits> for numeric_limits<>'
            '  [build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            '''#include <limits>
               int i = numeric_limits<int>::max()
            ''',
            '')

        # Test the UpdateIncludeState code path.
        mock_header_contents = ['#include "blah/foo.h"', '#include "blah/bar.h"']
        message = self.perform_include_what_you_use(
            '#include "config.h"\n'
            '#include "blah/a.h"\n',
            filename='blah/a.cpp',
            io=MockIo(mock_header_contents))
        self.assertEquals(message, '')

        mock_header_contents = ['#include <set>']
        message = self.perform_include_what_you_use(
            '''#include "config.h"
               #include "blah/a.h"

               std::set<int> foo;''',
            filename='blah/a.cpp',
            io=MockIo(mock_header_contents))
        self.assertEquals(message, '')

        # If there's just a .cpp and the header can't be found then it's ok.
        message = self.perform_include_what_you_use(
            '''#include "config.h"
               #include "blah/a.h"

               std::set<int> foo;''',
            filename='blah/a.cpp')
        self.assertEquals(message, '')

        # Make sure we find the headers with relative paths.
        mock_header_contents = ['']
        message = self.perform_include_what_you_use(
            '''#include "config.h"
               #include "%s/a.h"

               std::set<int> foo;''' % os.path.basename(os.getcwd()),
            filename='a.cpp',
            io=MockIo(mock_header_contents))
        self.assertEquals(message, 'Add #include <set> for set<>  '
                                   '[build/include_what_you_use] [4]')

    def test_files_belong_to_same_module(self):
        f = cpp_style.files_belong_to_same_module
        self.assertEquals((True, ''), f('a.cpp', 'a.h'))
        self.assertEquals((True, ''), f('base/google.cpp', 'base/google.h'))
        self.assertEquals((True, ''), f('base/google_test.cpp', 'base/google.h'))
        self.assertEquals((True, ''),
                          f('base/google_unittest.cpp', 'base/google.h'))
        self.assertEquals((True, ''),
                          f('base/internal/google_unittest.cpp',
                            'base/public/google.h'))
        self.assertEquals((True, 'xxx/yyy/'),
                          f('xxx/yyy/base/internal/google_unittest.cpp',
                            'base/public/google.h'))
        self.assertEquals((True, 'xxx/yyy/'),
                          f('xxx/yyy/base/google_unittest.cpp',
                            'base/public/google.h'))
        self.assertEquals((True, ''),
                          f('base/google_unittest.cpp', 'base/google-inl.h'))
        self.assertEquals((True, '/home/build/google3/'),
                          f('/home/build/google3/base/google.cpp', 'base/google.h'))

        self.assertEquals((False, ''),
                          f('/home/build/google3/base/google.cpp', 'basu/google.h'))
        self.assertEquals((False, ''), f('a.cpp', 'b.h'))

    def test_cleanse_line(self):
        self.assertEquals('int foo = 0;  ',
                          cpp_style.cleanse_comments('int foo = 0;  // danger!'))
        self.assertEquals('int o = 0;',
                          cpp_style.cleanse_comments('int /* foo */ o = 0;'))
        self.assertEquals('foo(int a, int b);',
                          cpp_style.cleanse_comments('foo(int a /* abc */, int b);'))
        self.assertEqual('f(a, b);',
                         cpp_style.cleanse_comments('f(a, /* name */ b);'))
        self.assertEqual('f(a, b);',
                         cpp_style.cleanse_comments('f(a /* name */, b);'))
        self.assertEqual('f(a, b);',
                         cpp_style.cleanse_comments('f(a, /* name */b);'))

    def test_multi_line_comments(self):
        # missing explicit is bad
        self.assert_multi_line_lint(
            r'''int a = 0;
                /* multi-liner
                class Foo {
                Foo(int f);  // should cause a lint warning in code
                }
            */ ''',
        '')
        self.assert_multi_line_lint(
            r'''/* int a = 0; multi-liner
            static const int b = 0;''',
      'Could not find end of multi-line comment'
      '  [readability/multiline_comment] [5]')
        self.assert_multi_line_lint(r'''    /* multi-line comment''',
                                    'Could not find end of multi-line comment'
                                    '  [readability/multiline_comment] [5]')
        self.assert_multi_line_lint(r'''    // /* comment, but not multi-line''', '')

    def test_multiline_strings(self):
        multiline_string_error_message = (
            'Multi-line string ("...") found.  This lint script doesn\'t '
            'do well with such strings, and may give bogus warnings.  They\'re '
            'ugly and unnecessary, and you should use concatenation instead".'
            '  [readability/multiline_string] [5]')

        file_path = 'mydir/foo.cpp'

        error_collector = ErrorCollector(self.assert_)
        cpp_style.process_file_data(file_path, 'cpp',
                                    ['const char* str = "This is a\\',
                                     ' multiline string.";'],
                                    error_collector)
        self.assertEquals(
            2,  # One per line.
            error_collector.result_list().count(multiline_string_error_message))

    # Test non-explicit single-argument constructors
    def test_explicit_single_argument_constructors(self):
        # missing explicit is bad
        self.assert_multi_line_lint(
            '''class Foo {
                 Foo(int f);
               };''',
            'Single-argument constructors should be marked explicit.'
            '  [runtime/explicit] [5]')
        # missing explicit is bad, even with whitespace
        self.assert_multi_line_lint(
            '''class Foo {
                 Foo (int f);
               };''',
            ['Extra space before ( in function call  [whitespace/parens] [4]',
             'Single-argument constructors should be marked explicit.'
             '  [runtime/explicit] [5]'])
        # missing explicit, with distracting comment, is still bad
        self.assert_multi_line_lint(
            '''class Foo {
                 Foo(int f);  // simpler than Foo(blargh, blarg)
               };''',
            'Single-argument constructors should be marked explicit.'
            '  [runtime/explicit] [5]')
        # missing explicit, with qualified classname
        self.assert_multi_line_lint(
            '''class Qualifier::AnotherOne::Foo {
                 Foo(int f);
               };''',
            'Single-argument constructors should be marked explicit.'
            '  [runtime/explicit] [5]')
        # structs are caught as well.
        self.assert_multi_line_lint(
            '''struct Foo {
                 Foo(int f);
               };''',
            'Single-argument constructors should be marked explicit.'
            '  [runtime/explicit] [5]')
        # Templatized classes are caught as well.
        self.assert_multi_line_lint(
            '''template<typename T> class Foo {
                 Foo(int f);
               };''',
            'Single-argument constructors should be marked explicit.'
            '  [runtime/explicit] [5]')
        # proper style is okay
        self.assert_multi_line_lint(
            '''class Foo {
                 explicit Foo(int f);
               };''',
            '')
        # two argument constructor is okay
        self.assert_multi_line_lint(
            '''class Foo {
                 Foo(int f, int b);
               };''',
            '')
        # two argument constructor, across two lines, is okay
        self.assert_multi_line_lint(
            '''class Foo {
                 Foo(int f,
                     int b);
               };''',
            '')
        # non-constructor (but similar name), is okay
        self.assert_multi_line_lint(
            '''class Foo {
                 aFoo(int f);
               };''',
            '')
        # constructor with void argument is okay
        self.assert_multi_line_lint(
            '''class Foo {
                 Foo(void);
               };''',
            '')
        # single argument method is okay
        self.assert_multi_line_lint(
            '''class Foo {
                 Bar(int b);
               };''',
            '')
        # comments should be ignored
        self.assert_multi_line_lint(
            '''class Foo {
               // Foo(int f);
               };''',
            '')
        # single argument function following class definition is okay
        # (okay, it's not actually valid, but we don't want a false positive)
        self.assert_multi_line_lint(
            '''class Foo {
                 Foo(int f, int b);
               };
               Foo(int f);''',
            '')
        # single argument function is okay
        self.assert_multi_line_lint(
            '''static Foo(int f);''',
            '')
        # single argument copy constructor is okay.
        self.assert_multi_line_lint(
            '''class Foo {
                 Foo(const Foo&);
               };''',
            '')
        self.assert_multi_line_lint(
            '''class Foo {
                 Foo(Foo&);
               };''',
            '')

    def test_slash_star_comment_on_single_line(self):
        self.assert_multi_line_lint(
            '''/* static */ Foo(int f);''',
            '')
        self.assert_multi_line_lint(
            '''/*/ static */  Foo(int f);''',
            '')
        self.assert_multi_line_lint(
            '''/*/ static Foo(int f);''',
            'Could not find end of multi-line comment'
            '  [readability/multiline_comment] [5]')
        self.assert_multi_line_lint(
            '''    /*/ static Foo(int f);''',
            'Could not find end of multi-line comment'
            '  [readability/multiline_comment] [5]')
        self.assert_multi_line_lint(
            '''    /**/ static Foo(int f);''',
            '')

    # Test suspicious usage of "if" like this:
    # if (a == b) {
    #   DoSomething();
    # } if (a == c) {   // Should be "else if".
    #   DoSomething();  // This gets called twice if a == b && a == c.
    # }
    def test_suspicious_usage_of_if(self):
        self.assert_lint(
            '    if (a == b) {',
            '')
        self.assert_lint(
            '    } if (a == b) {',
            'Did you mean "else if"? If not, start a new line for "if".'
            '  [readability/braces] [4]')

    # Test suspicious usage of memset. Specifically, a 0
    # as the final argument is almost certainly an error.
    def test_suspicious_usage_of_memset(self):
        # Normal use is okay.
        self.assert_lint(
            '    memset(buf, 0, sizeof(buf))',
            '')

        # A 0 as the final argument is almost certainly an error.
        self.assert_lint(
            '    memset(buf, sizeof(buf), 0)',
            'Did you mean "memset(buf, 0, sizeof(buf))"?'
            '  [runtime/memset] [4]')
        self.assert_lint(
            '    memset(buf, xsize * ysize, 0)',
            'Did you mean "memset(buf, 0, xsize * ysize)"?'
            '  [runtime/memset] [4]')

        # There is legitimate test code that uses this form.
        # This is okay since the second argument is a literal.
        self.assert_lint(
            "    memset(buf, 'y', 0)",
            '')
        self.assert_lint(
            '    memset(buf, 4, 0)',
            '')
        self.assert_lint(
            '    memset(buf, -1, 0)',
            '')
        self.assert_lint(
            '    memset(buf, 0xF1, 0)',
            '')
        self.assert_lint(
            '    memset(buf, 0xcd, 0)',
            '')

    def test_check_posix_threading(self):
        self.assert_lint('sctime_r()', '')
        self.assert_lint('strtok_r()', '')
        self.assert_lint('    strtok_r(foo, ba, r)', '')
        self.assert_lint('brand()', '')
        self.assert_lint('_rand()', '')
        self.assert_lint('.rand()', '')
        self.assert_lint('>rand()', '')
        self.assert_lint('rand()',
                         'Consider using rand_r(...) instead of rand(...)'
                         ' for improved thread safety.'
                         '  [runtime/threadsafe_fn] [2]')
        self.assert_lint('strtok()',
                         'Consider using strtok_r(...) '
                         'instead of strtok(...)'
                         ' for improved thread safety.'
                         '  [runtime/threadsafe_fn] [2]')

    # Test potential format string bugs like printf(foo).
    def test_format_strings(self):
        self.assert_lint('printf("foo")', '')
        self.assert_lint('printf("foo: %s", foo)', '')
        self.assert_lint('DocidForPrintf(docid)', '')  # Should not trigger.
        self.assert_lint(
            'printf(foo)',
            'Potential format string bug. Do printf("%s", foo) instead.'
            '  [runtime/printf] [4]')
        self.assert_lint(
            'printf(foo.c_str())',
            'Potential format string bug. '
            'Do printf("%s", foo.c_str()) instead.'
            '  [runtime/printf] [4]')
        self.assert_lint(
            'printf(foo->c_str())',
            'Potential format string bug. '
            'Do printf("%s", foo->c_str()) instead.'
            '  [runtime/printf] [4]')
        self.assert_lint(
            'StringPrintf(foo)',
            'Potential format string bug. Do StringPrintf("%s", foo) instead.'
            ''
            '  [runtime/printf] [4]')

    # Variable-length arrays are not permitted.
    def test_variable_length_array_detection(self):
        errmsg = ('Do not use variable-length arrays.  Use an appropriately named '
                  "('k' followed by CamelCase) compile-time constant for the size."
                  '  [runtime/arrays] [1]')

        self.assert_lint('int a[any_old_variable];', errmsg)
        self.assert_lint('int doublesize[some_var * 2];', errmsg)
        self.assert_lint('int a[afunction()];', errmsg)
        self.assert_lint('int a[function(kMaxFooBars)];', errmsg)
        self.assert_lint('bool a_list[items_->size()];', errmsg)
        self.assert_lint('namespace::Type buffer[len+1];', errmsg)

        self.assert_lint('int a[64];', '')
        self.assert_lint('int a[0xFF];', '')
        self.assert_lint('int first[256], second[256];', '')
        self.assert_lint('int array_name[kCompileTimeConstant];', '')
        self.assert_lint('char buf[somenamespace::kBufSize];', '')
        self.assert_lint('int array_name[ALL_CAPS];', '')
        self.assert_lint('AClass array1[foo::bar::ALL_CAPS];', '')
        self.assert_lint('int a[kMaxStrLen + 1];', '')
        self.assert_lint('int a[sizeof(foo)];', '')
        self.assert_lint('int a[sizeof(*foo)];', '')
        self.assert_lint('int a[sizeof foo];', '')
        self.assert_lint('int a[sizeof(struct Foo)];', '')
        self.assert_lint('int a[128 - sizeof(const bar)];', '')
        self.assert_lint('int a[(sizeof(foo) * 4)];', '')
        self.assert_lint('int a[(arraysize(fixed_size_array)/2) << 1];', 'Missing spaces around /  [whitespace/operators] [3]')
        self.assert_lint('delete a[some_var];', '')
        self.assert_lint('return a[some_var];', '')

    # Brace usage
    def test_braces(self):
        # Braces shouldn't be followed by a ; unless they're defining a struct
        # or initializing an array
        self.assert_lint('int a[3] = { 1, 2, 3 };', '')
        self.assert_lint(
            '''const int foo[] =
                   {1, 2, 3 };''',
            '')
        # For single line, unmatched '}' with a ';' is ignored (not enough context)
        self.assert_multi_line_lint(
            '''int a[3] = { 1,
                            2,
                            3 };''',
            '')
        self.assert_multi_line_lint(
            '''int a[2][3] = { { 1, 2 },
                             { 3, 4 } };''',
            '')
        self.assert_multi_line_lint(
            '''int a[2][3] =
                   { { 1, 2 },
                     { 3, 4 } };''',
            '')

    # CHECK/EXPECT_TRUE/EXPECT_FALSE replacements
    def test_check_check(self):
        self.assert_lint('CHECK(x == 42)',
                         'Consider using CHECK_EQ instead of CHECK(a == b)'
                         '  [readability/check] [2]')
        self.assert_lint('CHECK(x != 42)',
                         'Consider using CHECK_NE instead of CHECK(a != b)'
                         '  [readability/check] [2]')
        self.assert_lint('CHECK(x >= 42)',
                         'Consider using CHECK_GE instead of CHECK(a >= b)'
                         '  [readability/check] [2]')
        self.assert_lint('CHECK(x > 42)',
                         'Consider using CHECK_GT instead of CHECK(a > b)'
                         '  [readability/check] [2]')
        self.assert_lint('CHECK(x <= 42)',
                         'Consider using CHECK_LE instead of CHECK(a <= b)'
                         '  [readability/check] [2]')
        self.assert_lint('CHECK(x < 42)',
                         'Consider using CHECK_LT instead of CHECK(a < b)'
                         '  [readability/check] [2]')

        self.assert_lint('DCHECK(x == 42)',
                         'Consider using DCHECK_EQ instead of DCHECK(a == b)'
                         '  [readability/check] [2]')
        self.assert_lint('DCHECK(x != 42)',
                         'Consider using DCHECK_NE instead of DCHECK(a != b)'
                         '  [readability/check] [2]')
        self.assert_lint('DCHECK(x >= 42)',
                         'Consider using DCHECK_GE instead of DCHECK(a >= b)'
                         '  [readability/check] [2]')
        self.assert_lint('DCHECK(x > 42)',
                         'Consider using DCHECK_GT instead of DCHECK(a > b)'
                         '  [readability/check] [2]')
        self.assert_lint('DCHECK(x <= 42)',
                         'Consider using DCHECK_LE instead of DCHECK(a <= b)'
                         '  [readability/check] [2]')
        self.assert_lint('DCHECK(x < 42)',
                         'Consider using DCHECK_LT instead of DCHECK(a < b)'
                         '  [readability/check] [2]')

        self.assert_lint(
            'EXPECT_TRUE("42" == x)',
            'Consider using EXPECT_EQ instead of EXPECT_TRUE(a == b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'EXPECT_TRUE("42" != x)',
            'Consider using EXPECT_NE instead of EXPECT_TRUE(a != b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'EXPECT_TRUE(+42 >= x)',
            'Consider using EXPECT_GE instead of EXPECT_TRUE(a >= b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'EXPECT_TRUE_M(-42 > x)',
            'Consider using EXPECT_GT_M instead of EXPECT_TRUE_M(a > b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'EXPECT_TRUE_M(42U <= x)',
            'Consider using EXPECT_LE_M instead of EXPECT_TRUE_M(a <= b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'EXPECT_TRUE_M(42L < x)',
            'Consider using EXPECT_LT_M instead of EXPECT_TRUE_M(a < b)'
            '  [readability/check] [2]')

        self.assert_lint(
            'EXPECT_FALSE(x == 42)',
            'Consider using EXPECT_NE instead of EXPECT_FALSE(a == b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'EXPECT_FALSE(x != 42)',
            'Consider using EXPECT_EQ instead of EXPECT_FALSE(a != b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'EXPECT_FALSE(x >= 42)',
            'Consider using EXPECT_LT instead of EXPECT_FALSE(a >= b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'ASSERT_FALSE(x > 42)',
            'Consider using ASSERT_LE instead of ASSERT_FALSE(a > b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'ASSERT_FALSE(x <= 42)',
            'Consider using ASSERT_GT instead of ASSERT_FALSE(a <= b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'ASSERT_FALSE_M(x < 42)',
            'Consider using ASSERT_GE_M instead of ASSERT_FALSE_M(a < b)'
            '  [readability/check] [2]')

        self.assert_lint('CHECK(some_iterator == obj.end())', '')
        self.assert_lint('EXPECT_TRUE(some_iterator == obj.end())', '')
        self.assert_lint('EXPECT_FALSE(some_iterator == obj.end())', '')

        self.assert_lint('CHECK(CreateTestFile(dir, (1 << 20)));', '')
        self.assert_lint('CHECK(CreateTestFile(dir, (1 >> 20)));', '')

        self.assert_lint('CHECK(x<42)',
                         ['Missing spaces around <'
                          '  [whitespace/operators] [3]',
                          'Consider using CHECK_LT instead of CHECK(a < b)'
                          '  [readability/check] [2]'])
        self.assert_lint('CHECK(x>42)',
                         'Consider using CHECK_GT instead of CHECK(a > b)'
                         '  [readability/check] [2]')

        self.assert_lint(
            '    EXPECT_TRUE(42 < x)  // Random comment.',
            'Consider using EXPECT_LT instead of EXPECT_TRUE(a < b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'EXPECT_TRUE( 42 < x )',
            ['Extra space after ( in function call'
             '  [whitespace/parens] [4]',
             'Consider using EXPECT_LT instead of EXPECT_TRUE(a < b)'
             '  [readability/check] [2]'])
        self.assert_lint(
            'CHECK("foo" == "foo")',
            'Consider using CHECK_EQ instead of CHECK(a == b)'
            '  [readability/check] [2]')

        self.assert_lint('CHECK_EQ("foo", "foo")', '')

    def test_brace_at_begin_of_line(self):
        self.assert_lint('{',
                         'This { should be at the end of the previous line'
                         '  [whitespace/braces] [4]')
        self.assert_multi_line_lint(
            '#endif\n'
            '{\n'
            '}\n',
            '')
        self.assert_multi_line_lint(
            'if (condition) {',
            '')
        self.assert_multi_line_lint(
            'int foo() {',
            'Place brace on its own line for function definitions.  [whitespace/braces] [4]')
        self.assert_multi_line_lint(
            'int foo() const {',
            'Place brace on its own line for function definitions.  [whitespace/braces] [4]')
        self.assert_multi_line_lint(
            'int foo() const\n'
            '{\n'
            '}\n',
            '')
        self.assert_multi_line_lint(
            'if (condition\n'
            '    && condition2\n'
            '    && condition3) {\n'
            '}\n',
            '')

    def test_mismatching_spaces_in_parens(self):
        self.assert_lint('if (foo ) {', 'Mismatching spaces inside () in if'
                         '  [whitespace/parens] [5]')
        self.assert_lint('switch ( foo) {', 'Mismatching spaces inside () in switch'
                         '  [whitespace/parens] [5]')
        self.assert_lint('for (foo; ba; bar ) {', 'Mismatching spaces inside () in for'
                         '  [whitespace/parens] [5]')
        self.assert_lint('for (; foo; bar) {', '')
        self.assert_lint('for ( ; foo; bar) {', '')
        self.assert_lint('for ( ; foo; bar ) {', '')
        self.assert_lint('for (foo; bar; ) {', '')
        self.assert_lint('foreach (foo, foos ) {', 'Mismatching spaces inside () in foreach'
                         '  [whitespace/parens] [5]')
        self.assert_lint('foreach ( foo, foos) {', 'Mismatching spaces inside () in foreach'
                         '  [whitespace/parens] [5]')
        self.assert_lint('while (  foo  ) {', 'Should have zero or one spaces inside'
                         ' ( and ) in while  [whitespace/parens] [5]')

    def test_spacing_for_fncall(self):
        self.assert_lint('if (foo) {', '')
        self.assert_lint('for (foo;bar;baz) {', '')
        self.assert_lint('foreach (foo, foos) {', '')
        self.assert_lint('while (foo) {', '')
        self.assert_lint('switch (foo) {', '')
        self.assert_lint('new (RenderArena()) RenderInline(document())', '')
        self.assert_lint('foo( bar)', 'Extra space after ( in function call'
                         '  [whitespace/parens] [4]')
        self.assert_lint('foobar( \\', '')
        self.assert_lint('foobar(     \\', '')
        self.assert_lint('( a + b)', 'Extra space after ('
                         '  [whitespace/parens] [2]')
        self.assert_lint('((a+b))', '')
        self.assert_lint('foo (foo)', 'Extra space before ( in function call'
                         '  [whitespace/parens] [4]')
        self.assert_lint('typedef foo (*foo)(foo)', '')
        self.assert_lint('typedef foo (*foo12bar_)(foo)', '')
        self.assert_lint('typedef foo (Foo::*bar)(foo)', '')
        self.assert_lint('foo (Foo::*bar)(',
                         'Extra space before ( in function call'
                         '  [whitespace/parens] [4]')
        self.assert_lint('typedef foo (Foo::*bar)(', '')
        self.assert_lint('(foo)(bar)', '')
        self.assert_lint('Foo (*foo)(bar)', '')
        self.assert_lint('Foo (*foo)(Bar bar,', '')
        self.assert_lint('char (*p)[sizeof(foo)] = &foo', '')
        self.assert_lint('char (&ref)[sizeof(foo)] = &foo', '')
        self.assert_lint('const char32 (*table[])[6];', '')

    def test_spacing_before_braces(self):
        self.assert_lint('if (foo){', 'Missing space before {'
                         '  [whitespace/braces] [5]')
        self.assert_lint('for{', 'Missing space before {'
                         '  [whitespace/braces] [5]')
        self.assert_lint('for {', '')
        self.assert_lint('EXPECT_DEBUG_DEATH({', '')

    def test_spacing_around_else(self):
        self.assert_lint('}else {', 'Missing space before else'
                         '  [whitespace/braces] [5]')
        self.assert_lint('} else{', 'Missing space before {'
                         '  [whitespace/braces] [5]')
        self.assert_lint('} else {', '')
        self.assert_lint('} else if', '')

    def test_spacing_for_binary_ops(self):
        self.assert_lint('if (foo<=bar) {', 'Missing spaces around <='
                         '  [whitespace/operators] [3]')
        self.assert_lint('if (foo<bar) {', 'Missing spaces around <'
                         '  [whitespace/operators] [3]')
        self.assert_lint('if (foo<bar->baz) {', 'Missing spaces around <'
                         '  [whitespace/operators] [3]')
        self.assert_lint('if (foo<bar->bar) {', 'Missing spaces around <'
                         '  [whitespace/operators] [3]')
        self.assert_lint('typedef hash_map<Foo, Bar', 'Missing spaces around <'
                         '  [whitespace/operators] [3]')
        self.assert_lint('typedef hash_map<FoooooType, BaaaaarType,', '')
        self.assert_lint('a<Foo> t+=b;', 'Missing spaces around +='
                         '  [whitespace/operators] [3]')
        self.assert_lint('a<Foo> t-=b;', 'Missing spaces around -='
                         '  [whitespace/operators] [3]')
        self.assert_lint('a<Foo*> t*=b;', 'Missing spaces around *='
                         '  [whitespace/operators] [3]')
        self.assert_lint('a<Foo*> t/=b;', 'Missing spaces around /='
                         '  [whitespace/operators] [3]')
        self.assert_lint('a<Foo*> t|=b;', 'Missing spaces around |='
                         '  [whitespace/operators] [3]')
        self.assert_lint('a<Foo*> t&=b;', 'Missing spaces around &='
                         '  [whitespace/operators] [3]')
        self.assert_lint('a<Foo*> t<<=b;', 'Missing spaces around <<='
                         '  [whitespace/operators] [3]')
        self.assert_lint('a<Foo*> t>>=b;', 'Missing spaces around >>='
                         '  [whitespace/operators] [3]')
        self.assert_lint('a<Foo*> t>>=&b|c;', 'Missing spaces around >>='
                         '  [whitespace/operators] [3]')
        self.assert_lint('a<Foo*> t<<=*b/c;', 'Missing spaces around <<='
                         '  [whitespace/operators] [3]')
        self.assert_lint('a<Foo> t -= b;', '')
        self.assert_lint('a<Foo> t += b;', '')
        self.assert_lint('a<Foo*> t *= b;', '')
        self.assert_lint('a<Foo*> t /= b;', '')
        self.assert_lint('a<Foo*> t |= b;', '')
        self.assert_lint('a<Foo*> t &= b;', '')
        self.assert_lint('a<Foo*> t <<= b;', '')
        self.assert_lint('a<Foo*> t >>= b;', '')
        self.assert_lint('a<Foo*> t >>= &b|c;', 'Missing spaces around |'
                         '  [whitespace/operators] [3]')
        self.assert_lint('a<Foo*> t <<= *b/c;', 'Missing spaces around /'
                         '  [whitespace/operators] [3]')
        self.assert_lint('a<Foo*> t <<= b/c; //Test', ['At least two spaces'
                         ' is best between code and comments  [whitespace/'
                         'comments] [2]', 'Should have a space between // '
                         'and comment  [whitespace/comments] [4]', 'Missing'
                         ' spaces around /  [whitespace/operators] [3]'])
        self.assert_lint('a<Foo*> t <<= b||c;  //Test', ['Should have a space'
                         ' between // and comment  [whitespace/comments] [4]',
                         'Missing spaces around ||  [whitespace/operators] [3]'])
        self.assert_lint('a<Foo*> t <<= b&&c;  // Test', 'Missing spaces around'
                         ' &&  [whitespace/operators] [3]')
        self.assert_lint('a<Foo*> t <<= b&&&c;  // Test', 'Missing spaces around'
                         ' &&  [whitespace/operators] [3]')
        self.assert_lint('a<Foo*> t <<= b&&*c;  // Test', 'Missing spaces around'
                         ' &&  [whitespace/operators] [3]')
        self.assert_lint('a<Foo*> t <<= b && *c;  // Test', '')
        self.assert_lint('a<Foo*> t <<= b && &c;  // Test', '')
        self.assert_lint('a<Foo*> t <<= b || &c;  /*Test', 'Complex multi-line '
                         '/*...*/-style comment found. Lint may give bogus '
                         'warnings.  Consider replacing these with //-style'
                         ' comments, with #if 0...#endif, or with more clearly'
                         ' structured multi-line comments.  [readability/multiline_comment] [5]')
        self.assert_lint('a<Foo&> t <<= &b | &c;', '')
        self.assert_lint('a<Foo*> t <<= &b & &c;  // Test', '')
        self.assert_lint('a<Foo*> t <<= *b / &c;  // Test', '')
        self.assert_lint('if (a=b == 1)', 'Missing spaces around =  [whitespace/operators] [4]')
        self.assert_lint('a = 1<<20', 'Missing spaces around <<  [whitespace/operators] [3]')
        self.assert_lint('if (a = b == 1)', '')
        self.assert_lint('a = 1 << 20', '')
        self.assert_multi_line_lint('#include "config.h"\n#include <sys/io.h>\n',
                                    '')

    def test_spacing_before_last_semicolon(self):
        self.assert_lint('call_function() ;',
                         'Extra space before last semicolon. If this should be an '
                         'empty statement, use { } instead.'
                         '  [whitespace/semicolon] [5]')
        self.assert_lint('while (true) ;',
                         'Extra space before last semicolon. If this should be an '
                         'empty statement, use { } instead.'
                         '  [whitespace/semicolon] [5]')
        self.assert_lint('default:;',
                         'Semicolon defining empty statement. Use { } instead.'
                         '  [whitespace/semicolon] [5]')
        self.assert_lint('      ;',
                         'Line contains only semicolon. If this should be an empty '
                         'statement, use { } instead.'
                         '  [whitespace/semicolon] [5]')
        self.assert_lint('for (int i = 0; ;', '')

    # Static or global STL strings.
    def test_static_or_global_stlstrings(self):
        self.assert_lint('string foo;',
                         'For a static/global string constant, use a C style '
                         'string instead: "char foo[]".'
                         '  [runtime/string] [4]')
        self.assert_lint('string kFoo = "hello";  // English',
                         'For a static/global string constant, use a C style '
                         'string instead: "char kFoo[]".'
                         '  [runtime/string] [4]')
        self.assert_lint('static string foo;',
                         'For a static/global string constant, use a C style '
                         'string instead: "static char foo[]".'
                         '  [runtime/string] [4]')
        self.assert_lint('static const string foo;',
                         'For a static/global string constant, use a C style '
                         'string instead: "static const char foo[]".'
                         '  [runtime/string] [4]')
        self.assert_lint('string Foo::bar;',
                         'For a static/global string constant, use a C style '
                         'string instead: "char Foo::bar[]".'
                         '  [runtime/string] [4]')
        # Rare case.
        self.assert_lint('string foo("foobar");',
                         'For a static/global string constant, use a C style '
                         'string instead: "char foo[]".'
                         '  [runtime/string] [4]')
        # Should not catch local or member variables.
        self.assert_lint('    string foo', '')
        # Should not catch functions.
        self.assert_lint('string EmptyString() { return ""; }', '')
        self.assert_lint('string EmptyString () { return ""; }', '')
        self.assert_lint('string VeryLongNameFunctionSometimesEndsWith(\n'
                         '    VeryLongNameType very_long_name_variable) {}', '')
        self.assert_lint('template<>\n'
                         'string FunctionTemplateSpecialization<SomeType>(\n'
                         '      int x) { return ""; }', '')
        self.assert_lint('template<>\n'
                         'string FunctionTemplateSpecialization<vector<A::B>* >(\n'
                         '      int x) { return ""; }', '')

        # should not catch methods of template classes.
        self.assert_lint('string Class<Type>::Method() const\n'
                         '{\n'
                         '  return "";\n'
                         '}\n', '')
        self.assert_lint('string Class<Type>::Method(\n'
                         '   int arg) const\n'
                         '{\n'
                         '  return "";\n'
                         '}\n', '')

    def test_no_spaces_in_function_calls(self):
        self.assert_lint('TellStory(1, 3);',
                         '')
        self.assert_lint('TellStory(1, 3 );',
                         'Extra space before )'
                         '  [whitespace/parens] [2]')
        self.assert_lint('TellStory(1 /* wolf */, 3 /* pigs */);',
                         '')
        self.assert_multi_line_lint('#endif\n    );',
                                    '')

    def test_two_spaces_between_code_and_comments(self):
        self.assert_lint('} // namespace foo',
                         'At least two spaces is best between code and comments'
                         '  [whitespace/comments] [2]')
        self.assert_lint('}// namespace foo',
                         'At least two spaces is best between code and comments'
                         '  [whitespace/comments] [2]')
        self.assert_lint('printf("foo"); // Outside quotes.',
                         'At least two spaces is best between code and comments'
                         '  [whitespace/comments] [2]')
        self.assert_lint('int i = 0;  // Having two spaces is fine.', '')
        self.assert_lint('int i = 0;   // Having three spaces is OK.', '')
        self.assert_lint('// Top level comment', '')
        self.assert_lint('    // Line starts with four spaces.', '')
        self.assert_lint('foo();\n'
                         '{ // A scope is opening.', '')
        self.assert_lint('    foo();\n'
                         '    { // An indented scope is opening.', '')
        self.assert_lint('if (foo) { // not a pure scope; comment is too close!',
                         'At least two spaces is best between code and comments'
                         '  [whitespace/comments] [2]')
        self.assert_lint('printf("// In quotes.")', '')
        self.assert_lint('printf("\\"%s // In quotes.")', '')
        self.assert_lint('printf("%s", "// In quotes.")', '')

    def test_space_after_comment_marker(self):
        self.assert_lint('//', '')
        self.assert_lint('//x', 'Should have a space between // and comment'
                         '  [whitespace/comments] [4]')
        self.assert_lint('// x', '')
        self.assert_lint('//----', '')
        self.assert_lint('//====', '')
        self.assert_lint('//////', '')
        self.assert_lint('////// x', '')
        self.assert_lint('/// x', '')
        self.assert_lint('////x', 'Should have a space between // and comment'
                         '  [whitespace/comments] [4]')

    def test_newline_at_eof(self):
        def do_test(self, data, is_missing_eof):
            error_collector = ErrorCollector(self.assert_)
            cpp_style.process_file_data('foo.cpp', 'cpp', data.split('\n'),
                                        error_collector)
            # The warning appears only once.
            self.assertEquals(
                int(is_missing_eof),
                error_collector.results().count(
                    'Could not find a newline character at the end of the file.'
                    '  [whitespace/ending_newline] [5]'))

        do_test(self, '// Newline\n// at EOF\n', False)
        do_test(self, '// No newline\n// at EOF', True)

    def test_invalid_utf8(self):
        def do_test(self, raw_bytes, has_invalid_utf8):
            error_collector = ErrorCollector(self.assert_)
            cpp_style.process_file_data(
                'foo.cpp', 'cpp',
                unicode(raw_bytes, 'utf8', 'replace').split('\n'),
                error_collector)
            # The warning appears only once.
            self.assertEquals(
                int(has_invalid_utf8),
                error_collector.results().count(
                    'Line contains invalid UTF-8'
                    ' (or Unicode replacement character).'
                    '  [readability/utf8] [5]'))

        do_test(self, 'Hello world\n', False)
        do_test(self, '\xe9\x8e\xbd\n', False)
        do_test(self, '\xe9x\x8e\xbd\n', True)
        # This is the encoding of the replacement character itself (which
        # you can see by evaluating codecs.getencoder('utf8')(u'\ufffd')).
        do_test(self, '\xef\xbf\xbd\n', True)

    def test_is_blank_line(self):
        self.assert_(cpp_style.is_blank_line(''))
        self.assert_(cpp_style.is_blank_line(' '))
        self.assert_(cpp_style.is_blank_line(' \t\r\n'))
        self.assert_(not cpp_style.is_blank_line('int a;'))
        self.assert_(not cpp_style.is_blank_line('{'))

    def test_blank_lines_check(self):
        self.assert_blank_lines_check(['{\n', '\n', '\n', '}\n'], 1, 1)
        self.assert_blank_lines_check(['  if (foo) {\n', '\n', '  }\n'], 1, 1)
        self.assert_blank_lines_check(
            ['\n', '// {\n', '\n', '\n', '// Comment\n', '{\n', '}\n'], 0, 0)
        self.assert_blank_lines_check(['\n', 'run("{");\n', '\n'], 0, 0)
        self.assert_blank_lines_check(['\n', '  if (foo) { return 0; }\n', '\n'], 0, 0)

    def test_allow_blank_line_before_closing_namespace(self):
        error_collector = ErrorCollector(self.assert_)
        cpp_style.process_file_data('foo.cpp', 'cpp',
                                    ['namespace {', '', '}  // namespace'],
                                    error_collector)
        self.assertEquals(0, error_collector.results().count(
            'Blank line at the end of a code block.  Is this needed?'
            '  [whitespace/blank_line] [3]'))

    def test_allow_blank_line_before_if_else_chain(self):
        error_collector = ErrorCollector(self.assert_)
        cpp_style.process_file_data('foo.cpp', 'cpp',
                                    ['if (hoge) {',
                                     '',  # No warning
                                     '} else if (piyo) {',
                                     '',  # No warning
                                     '} else if (piyopiyo) {',
                                     '  hoge = true;',  # No warning
                                     '} else {',
                                     '',  # Warning on this line
                                     '}'],
                                    error_collector)
        self.assertEquals(1, error_collector.results().count(
            'Blank line at the end of a code block.  Is this needed?'
            '  [whitespace/blank_line] [3]'))

    def test_else_on_same_line_as_closing_braces(self):
        error_collector = ErrorCollector(self.assert_)
        cpp_style.process_file_data('foo.cpp', 'cpp',
                                    ['if (hoge) {',
                                     '',
                                     '}',
                                     ' else {'  # Warning on this line
                                     '',
                                     '}'],
                                    error_collector)
        self.assertEquals(1, error_collector.results().count(
            'An else should appear on the same line as the preceding }'
            '  [whitespace/newline] [4]'))

    def test_else_clause_not_on_same_line_as_else(self):
        self.assert_lint('    else DoSomethingElse();',
                         'Else clause should never be on same line as else '
                         '(use 2 lines)  [whitespace/newline] [4]')
        self.assert_lint('    else ifDoSomethingElse();',
                         'Else clause should never be on same line as else '
                         '(use 2 lines)  [whitespace/newline] [4]')
        self.assert_lint('    else if (blah) {', '')
        self.assert_lint('    variable_ends_in_else = true;', '')

    def test_comma(self):
        self.assert_lint('a = f(1,2);',
                         'Missing space after ,  [whitespace/comma] [3]')
        self.assert_lint('int tmp=a,a=b,b=tmp;',
                         ['Missing spaces around =  [whitespace/operators] [4]',
                          'Missing space after ,  [whitespace/comma] [3]'])
        self.assert_lint('f(a, /* name */ b);', '')
        self.assert_lint('f(a, /* name */b);', '')

    def test_pointer_reference_marker_location(self):
        self.assert_lint('int* b;', '', 'foo.cpp')
        self.assert_lint('int *b;',
                         'Declaration has space between type name and * in int *b  [whitespace/declaration] [3]',
                         'foo.cpp')
        self.assert_lint('return *b;', '', 'foo.cpp')
        self.assert_lint('int *b;', '', 'foo.c')
        self.assert_lint('int* b;',
                         'Declaration has space between * and variable name in int* b  [whitespace/declaration] [3]',
                         'foo.c')
        self.assert_lint('int& b;', '', 'foo.cpp')
        self.assert_lint('int &b;',
                         'Declaration has space between type name and & in int &b  [whitespace/declaration] [3]',
                         'foo.cpp')
        self.assert_lint('return &b;', '', 'foo.cpp')

    def test_indent(self):
        self.assert_lint('static int noindent;', '')
        self.assert_lint('    int four_space_indent;', '')
        self.assert_lint(' int one_space_indent;',
                         'Weird number of spaces at line-start.  '
                         'Are you using a 4-space indent?  [whitespace/indent] [3]')
        self.assert_lint('   int three_space_indent;',
                         'Weird number of spaces at line-start.  '
                         'Are you using a 4-space indent?  [whitespace/indent] [3]')
        self.assert_lint(' char* one_space_indent = "public:";',
                         'Weird number of spaces at line-start.  '
                         'Are you using a 4-space indent?  [whitespace/indent] [3]')
        self.assert_lint(' public:', '')
        self.assert_lint('  public:', '')
        self.assert_lint('   public:', '')

    def test_label(self):
        self.assert_lint('public:',
                         'Labels should always be indented at least one space.  '
                         'If this is a member-initializer list in a constructor, '
                         'the colon should be on the line after the definition '
                         'header.  [whitespace/labels] [4]')
        self.assert_lint('  public:', '')
        self.assert_lint('   public:', '')
        self.assert_lint(' public:', '')
        self.assert_lint('  public:', '')
        self.assert_lint('   public:', '')

    def test_not_alabel(self):
        self.assert_lint('MyVeryLongNamespace::MyVeryLongClassName::', '')

    def test_tab(self):
        self.assert_lint('\tint a;',
                         'Tab found; better to use spaces  [whitespace/tab] [1]')
        self.assert_lint('int a = 5;\t\t// set a to 5',
                         'Tab found; better to use spaces  [whitespace/tab] [1]')

    def test_parse_arguments(self):
        old_usage = cpp_style._USAGE
        old_error_categories = cpp_style._ERROR_CATEGORIES
        old_output_format = cpp_style._cpp_style_state.output_format
        old_verbose_level = cpp_style._cpp_style_state.verbose_level
        old_filters = cpp_style._cpp_style_state.filters
        try:
            # Don't print usage during the tests, or filter categories
            cpp_style._USAGE = ''
            cpp_style._ERROR_CATEGORIES = ''

            self.assertRaises(SystemExit, cpp_style.parse_arguments, ['--badopt'])
            self.assertRaises(SystemExit, cpp_style.parse_arguments, ['--help'])
            self.assertRaises(SystemExit, cpp_style.parse_arguments, ['--filter='])
            # This is illegal because all filters must start with + or -
            self.assertRaises(ValueError, cpp_style.parse_arguments, ['--filter=foo'])
            self.assertRaises(ValueError, cpp_style.parse_arguments,
                              ['--filter=+a,b,-c'])

            self.assertEquals((['foo.cpp'], {}), cpp_style.parse_arguments(['foo.cpp']))
            self.assertEquals(old_output_format, cpp_style._cpp_style_state.output_format)
            self.assertEquals(old_verbose_level, cpp_style._cpp_style_state.verbose_level)

            self.assertEquals(([], {}), cpp_style.parse_arguments([]))
            self.assertEquals(([], {}), cpp_style.parse_arguments(['--v=0']))

            self.assertEquals((['foo.cpp'], {}),
                              cpp_style.parse_arguments(['--v=1', 'foo.cpp']))
            self.assertEquals(1, cpp_style._cpp_style_state.verbose_level)
            self.assertEquals((['foo.h'], {}),
                              cpp_style.parse_arguments(['--v=3', 'foo.h']))
            self.assertEquals(3, cpp_style._cpp_style_state.verbose_level)
            self.assertEquals((['foo.cpp'], {}),
                              cpp_style.parse_arguments(['--verbose=5', 'foo.cpp']))
            self.assertEquals(5, cpp_style._cpp_style_state.verbose_level)
            self.assertRaises(ValueError,
                              cpp_style.parse_arguments, ['--v=f', 'foo.cpp'])

            self.assertEquals((['foo.cpp'], {}),
                              cpp_style.parse_arguments(['--output=emacs', 'foo.cpp']))
            self.assertEquals('emacs', cpp_style._cpp_style_state.output_format)
            self.assertEquals((['foo.h'], {}),
                              cpp_style.parse_arguments(['--output=vs7', 'foo.h']))
            self.assertEquals('vs7', cpp_style._cpp_style_state.output_format)
            self.assertRaises(SystemExit,
                              cpp_style.parse_arguments, ['--output=blah', 'foo.cpp'])

            filt = '-,+whitespace,-whitespace/indent'
            self.assertEquals((['foo.h'], {}),
                              cpp_style.parse_arguments(['--filter='+filt, 'foo.h']))
            self.assertEquals(['-', '+whitespace', '-whitespace/indent'],
                              cpp_style._cpp_style_state.filters)

            self.assertEquals((['foo.cpp', 'foo.h'], {}),
                              cpp_style.parse_arguments(['foo.cpp', 'foo.h']))

            self.assertEquals((['foo.cpp'], {'--foo': ''}),
                              cpp_style.parse_arguments(['--foo', 'foo.cpp'], ['foo']))
            self.assertEquals((['foo.cpp'], {'--foo': 'bar'}),
                              cpp_style.parse_arguments(['--foo=bar', 'foo.cpp'], ['foo=']))
            self.assertEquals((['foo.cpp'], {}),
                              cpp_style.parse_arguments(['foo.cpp'], ['foo=']))
            self.assertRaises(SystemExit,
                              cpp_style.parse_arguments,
                              ['--footypo=bar', 'foo.cpp'], ['foo='])
        finally:
            cpp_style._USAGE = old_usage
            cpp_style._ERROR_CATEGORIES = old_error_categories
            cpp_style._cpp_style_state.output_format = old_output_format
            cpp_style._cpp_style_state.verbose_level = old_verbose_level
            cpp_style._cpp_style_state.filters = old_filters

    def test_filter(self):
        old_filters = cpp_style._cpp_style_state.filters
        try:
            cpp_style._cpp_style_state.set_filters('-,+whitespace,-whitespace/indent')
            self.assert_lint(
                '// Hello there ',
                'Line ends in whitespace.  Consider deleting these extra spaces.'
                '  [whitespace/end_of_line] [4]')
            self.assert_lint('int a = (int)1.0;', '')
            self.assert_lint(' weird opening space', '')
        finally:
            cpp_style._cpp_style_state.filters = old_filters

    def test_default_filter(self):
        default_filters = cpp_style._DEFAULT_FILTERS
        old_filters = cpp_style._cpp_style_state.filters
        cpp_style._DEFAULT_FILTERS = [ '-whitespace' ]
        try:
            # Reset filters
            cpp_style._cpp_style_state.set_filters('')
            self.assert_lint('// Hello there ', '')
            cpp_style._cpp_style_state.set_filters('+whitespace/end_of_line')
            self.assert_lint(
                '// Hello there ',
                'Line ends in whitespace.  Consider deleting these extra spaces.'
                '  [whitespace/end_of_line] [4]')
            self.assert_lint(' weird opening space', '')
        finally:
            cpp_style._cpp_style_state.filters = old_filters
            cpp_style._DEFAULT_FILTERS = default_filters

    def test_unnamed_namespaces_in_headers(self):
        self.assert_language_rules_check(
            'foo.h', 'namespace {',
            'Do not use unnamed namespaces in header files.  See'
            ' http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml#Namespaces'
            ' for more information.  [build/namespaces] [4]')
        # namespace registration macros are OK.
        self.assert_language_rules_check('foo.h', 'namespace {  \\', '')
        # named namespaces are OK.
        self.assert_language_rules_check('foo.h', 'namespace foo {', '')
        self.assert_language_rules_check('foo.h', 'namespace foonamespace {', '')
        self.assert_language_rules_check('foo.cpp', 'namespace {', '')
        self.assert_language_rules_check('foo.cpp', 'namespace foo {', '')

    def test_build_class(self):
        # Test that the linter can parse to the end of class definitions,
        # and that it will report when it can't.
        # Use multi-line linter because it performs the ClassState check.
        self.assert_multi_line_lint(
            'class Foo {',
            'Failed to find complete declaration of class Foo'
            '  [build/class] [5]')
        # Don't warn on forward declarations of various types.
        self.assert_multi_line_lint(
            'class Foo;',
            '')
        self.assert_multi_line_lint(
            '''struct Foo*
                 foo = NewFoo();''',
            '')
        # Here is an example where the linter gets confused, even though
        # the code doesn't violate the style guide.
        self.assert_multi_line_lint(
            '''class Foo
            #ifdef DERIVE_FROM_GOO
              : public Goo {
            #else
              : public Hoo {
            #endif
              };''',
            'Failed to find complete declaration of class Foo'
            '  [build/class] [5]')

    def test_build_end_comment(self):
        # The crosstool compiler we currently use will fail to compile the
        # code in this test, so we might consider removing the lint check.
        self.assert_lint('#endif Not a comment',
                         'Uncommented text after #endif is non-standard.'
                         '  Use a comment.'
                         '  [build/endif_comment] [5]')

    def test_build_forward_decl(self):
        # The crosstool compiler we currently use will fail to compile the
        # code in this test, so we might consider removing the lint check.
        self.assert_lint('class Foo::Goo;',
                         'Inner-style forward declarations are invalid.'
                         '  Remove this line.'
                         '  [build/forward_decl] [5]')

    def test_build_header_guard(self):
        file_path = 'mydir/foo.h'

        # We can't rely on our internal stuff to get a sane path on the open source
        # side of things, so just parse out the suggested header guard. This
        # doesn't allow us to test the suggested header guard, but it does let us
        # test all the other header tests.
        error_collector = ErrorCollector(self.assert_)
        cpp_style.process_file_data(file_path, 'h', [], error_collector)
        expected_guard = ''
        matcher = re.compile(
            'No \#ifndef header guard found\, suggested CPP variable is\: ([A-Z_0-9]+) ')
        for error in error_collector.result_list():
            matches = matcher.match(error)
            if matches:
                expected_guard = matches.group(1)
                break

        # Make sure we extracted something for our header guard.
        self.assertNotEqual(expected_guard, '')

        # Wrong guard
        error_collector = ErrorCollector(self.assert_)
        cpp_style.process_file_data(file_path, 'h',
                                    ['#ifndef FOO_H', '#define FOO_H'], error_collector)
        self.assertEquals(
            1,
            error_collector.result_list().count(
                '#ifndef header guard has wrong style, please use: %s'
                '  [build/header_guard] [5]' % expected_guard),
            error_collector.result_list())

        # No define
        error_collector = ErrorCollector(self.assert_)
        cpp_style.process_file_data(file_path, 'h',
                                    ['#ifndef %s' % expected_guard], error_collector)
        self.assertEquals(
            1,
            error_collector.result_list().count(
                'No #ifndef header guard found, suggested CPP variable is: %s'
                '  [build/header_guard] [5]' % expected_guard),
            error_collector.result_list())

        # Mismatched define
        error_collector = ErrorCollector(self.assert_)
        cpp_style.process_file_data(file_path, 'h',
                                    ['#ifndef %s' % expected_guard,
                                     '#define FOO_H'],
                                    error_collector)
        self.assertEquals(
            1,
            error_collector.result_list().count(
                'No #ifndef header guard found, suggested CPP variable is: %s'
                '  [build/header_guard] [5]' % expected_guard),
            error_collector.result_list())

        # No endif
        error_collector = ErrorCollector(self.assert_)
        cpp_style.process_file_data(file_path, 'h',
                                    ['#ifndef %s' % expected_guard,
                                     '#define %s' % expected_guard],
                                    error_collector)
        self.assertEquals(
            1,
            error_collector.result_list().count(
                '#endif line should be "#endif  // %s"'
                '  [build/header_guard] [5]' % expected_guard),
            error_collector.result_list())

        # Commentless endif
        error_collector = ErrorCollector(self.assert_)
        cpp_style.process_file_data(file_path, 'h',
                                    ['#ifndef %s' % expected_guard,
                                     '#define %s' % expected_guard,
                                     '#endif'],
                                    error_collector)
        self.assertEquals(
            1,
            error_collector.result_list().count(
                '#endif line should be "#endif  // %s"'
                '  [build/header_guard] [5]' % expected_guard),
            error_collector.result_list())

        # Commentless endif for old-style guard
        error_collector = ErrorCollector(self.assert_)
        cpp_style.process_file_data(file_path, 'h',
                                    ['#ifndef %s_' % expected_guard,
                                     '#define %s_' % expected_guard,
                                     '#endif'],
                                    error_collector)
        self.assertEquals(
            1,
            error_collector.result_list().count(
                '#endif line should be "#endif  // %s"'
                '  [build/header_guard] [5]' % expected_guard),
            error_collector.result_list())

        # No header guard errors
        error_collector = ErrorCollector(self.assert_)
        cpp_style.process_file_data(file_path, 'h',
                                    ['#ifndef %s' % expected_guard,
                                     '#define %s' % expected_guard,
                                     '#endif  // %s' % expected_guard],
                                    error_collector)
        for line in error_collector.result_list():
            if line.find('build/header_guard') != -1:
                self.fail('Unexpected error: %s' % line)

        # No header guard errors for old-style guard
        error_collector = ErrorCollector(self.assert_)
        cpp_style.process_file_data(file_path, 'h',
                                    ['#ifndef %s_' % expected_guard,
                                     '#define %s_' % expected_guard,
                                     '#endif  // %s_' % expected_guard],
                                    error_collector)
        for line in error_collector.result_list():
            if line.find('build/header_guard') != -1:
                self.fail('Unexpected error: %s' % line)

        old_verbose_level = cpp_style._cpp_style_state.verbose_level
        try:
            cpp_style._cpp_style_state.verbose_level = 0
            # Warn on old-style guard if verbosity is 0.
            error_collector = ErrorCollector(self.assert_)
            cpp_style.process_file_data(file_path, 'h',
                                        ['#ifndef %s_' % expected_guard,
                                         '#define %s_' % expected_guard,
                                         '#endif  // %s_' % expected_guard],
                                        error_collector)
            self.assertEquals(
                1,
                error_collector.result_list().count(
                    '#ifndef header guard has wrong style, please use: %s'
                    '  [build/header_guard] [0]' % expected_guard),
                error_collector.result_list())
        finally:
            cpp_style._cpp_style_state.verbose_level = old_verbose_level

        # Completely incorrect header guard
        error_collector = ErrorCollector(self.assert_)
        cpp_style.process_file_data(file_path, 'h',
                                    ['#ifndef FOO',
                                     '#define FOO',
                                     '#endif  // FOO'],
                                    error_collector)
        self.assertEquals(
            1,
            error_collector.result_list().count(
                '#ifndef header guard has wrong style, please use: %s'
                '  [build/header_guard] [5]' % expected_guard),
            error_collector.result_list())
        self.assertEquals(
            1,
            error_collector.result_list().count(
                '#endif line should be "#endif  // %s"'
                '  [build/header_guard] [5]' % expected_guard),
            error_collector.result_list())

    def test_build_printf_format(self):
        self.assert_lint(
            r'printf("\%%d", value);',
            '%, [, (, and { are undefined character escapes.  Unescape them.'
            '  [build/printf_format] [3]')

        self.assert_lint(
            r'snprintf(buffer, sizeof(buffer), "\[%d", value);',
            '%, [, (, and { are undefined character escapes.  Unescape them.'
            '  [build/printf_format] [3]')

        self.assert_lint(
            r'fprintf(file, "\(%d", value);',
            '%, [, (, and { are undefined character escapes.  Unescape them.'
            '  [build/printf_format] [3]')

        self.assert_lint(
            r'vsnprintf(buffer, sizeof(buffer), "\\\{%d", ap);',
            '%, [, (, and { are undefined character escapes.  Unescape them.'
            '  [build/printf_format] [3]')

        # Don't warn if double-slash precedes the symbol
        self.assert_lint(r'printf("\\%%%d", value);',
                         '')

    def test_runtime_printf_format(self):
        self.assert_lint(
            r'fprintf(file, "%q", value);',
            '%q in format strings is deprecated.  Use %ll instead.'
            '  [runtime/printf_format] [3]')

        self.assert_lint(
            r'aprintf(file, "The number is %12q", value);',
            '%q in format strings is deprecated.  Use %ll instead.'
            '  [runtime/printf_format] [3]')

        self.assert_lint(
            r'printf(file, "The number is" "%-12q", value);',
            '%q in format strings is deprecated.  Use %ll instead.'
            '  [runtime/printf_format] [3]')

        self.assert_lint(
            r'printf(file, "The number is" "%+12q", value);',
            '%q in format strings is deprecated.  Use %ll instead.'
            '  [runtime/printf_format] [3]')

        self.assert_lint(
            r'printf(file, "The number is" "% 12q", value);',
            '%q in format strings is deprecated.  Use %ll instead.'
            '  [runtime/printf_format] [3]')

        self.assert_lint(
            r'snprintf(file, "Never mix %d and %1$d parmaeters!", value);',
            '%N$ formats are unconventional.  Try rewriting to avoid them.'
            '  [runtime/printf_format] [2]')

    def assert_lintLogCodeOnError(self, code, expected_message):
        # Special assert_lint which logs the input code on error.
        result = self.perform_single_line_lint(code, 'foo.cpp')
        if result != expected_message:
            self.fail('For code: "%s"\nGot: "%s"\nExpected: "%s"'
                      % (code, result, expected_message))

    def test_build_storage_class(self):
        qualifiers = [None, 'const', 'volatile']
        signs = [None, 'signed', 'unsigned']
        types = ['void', 'char', 'int', 'float', 'double',
                 'schar', 'int8', 'uint8', 'int16', 'uint16',
                 'int32', 'uint32', 'int64', 'uint64']
        storage_classes = ['auto', 'extern', 'register', 'static', 'typedef']

        build_storage_class_error_message = (
            'Storage class (static, extern, typedef, etc) should be first.'
            '  [build/storage_class] [5]')

        # Some explicit cases. Legal in C++, deprecated in C99.
        self.assert_lint('const int static foo = 5;',
                         build_storage_class_error_message)

        self.assert_lint('char static foo;',
                         build_storage_class_error_message)

        self.assert_lint('double const static foo = 2.0;',
                         build_storage_class_error_message)

        self.assert_lint('uint64 typedef unsigned_long_long;',
                         build_storage_class_error_message)

        self.assert_lint('int register foo = 0;',
                         build_storage_class_error_message)

        # Since there are a very large number of possibilities, randomly
        # construct declarations.
        # Make sure that the declaration is logged if there's an error.
        # Seed generator with an integer for absolute reproducibility.
        random.seed(25)
        for unused_i in range(10):
            # Build up random list of non-storage-class declaration specs.
            other_decl_specs = [random.choice(qualifiers), random.choice(signs),
                                random.choice(types)]
            # remove None
            other_decl_specs = filter(lambda x: x is not None, other_decl_specs)

            # shuffle
            random.shuffle(other_decl_specs)

            # insert storage class after the first
            storage_class = random.choice(storage_classes)
            insertion_point = random.randint(1, len(other_decl_specs))
            decl_specs = (other_decl_specs[0:insertion_point]
                          + [storage_class]
                          + other_decl_specs[insertion_point:])

            self.assert_lintLogCodeOnError(
                ' '.join(decl_specs) + ';',
                build_storage_class_error_message)

            # but no error if storage class is first
            self.assert_lintLogCodeOnError(
                storage_class + ' ' + ' '.join(other_decl_specs),
                '')

    def test_legal_copyright(self):
        legal_copyright_message = (
            'No copyright message found.  '
            'You should have a line: "Copyright [year] <Copyright Owner>"'
            '  [legal/copyright] [5]')

        copyright_line = '// Copyright 2008 Google Inc. All Rights Reserved.'

        file_path = 'mydir/googleclient/foo.cpp'

        # There should be a copyright message in the first 10 lines
        error_collector = ErrorCollector(self.assert_)
        cpp_style.process_file_data(file_path, 'cpp', [], error_collector)
        self.assertEquals(
            1,
            error_collector.result_list().count(legal_copyright_message))

        error_collector = ErrorCollector(self.assert_)
        cpp_style.process_file_data(
            file_path, 'cpp',
            ['' for unused_i in range(10)] + [copyright_line],
            error_collector)
        self.assertEquals(
            1,
            error_collector.result_list().count(legal_copyright_message))

        # Test that warning isn't issued if Copyright line appears early enough.
        error_collector = ErrorCollector(self.assert_)
        cpp_style.process_file_data(file_path, 'cpp', [copyright_line], error_collector)
        for message in error_collector.result_list():
            if message.find('legal/copyright') != -1:
                self.fail('Unexpected error: %s' % message)

        error_collector = ErrorCollector(self.assert_)
        cpp_style.process_file_data(
            file_path, 'cpp',
            ['' for unused_i in range(9)] + [copyright_line],
            error_collector)
        for message in error_collector.result_list():
            if message.find('legal/copyright') != -1:
                self.fail('Unexpected error: %s' % message)

    def test_invalid_increment(self):
        self.assert_lint('*count++;',
                         'Changing pointer instead of value (or unused value of '
                         'operator*).  [runtime/invalid_increment] [5]')

class CleansedLinesTest(unittest.TestCase):
    def test_init(self):
        lines = ['Line 1',
                 'Line 2',
                 'Line 3 // Comment test',
                 'Line 4 "foo"']

        clean_lines = cpp_style.CleansedLines(lines)
        self.assertEquals(lines, clean_lines.raw_lines)
        self.assertEquals(4, clean_lines.num_lines())

        self.assertEquals(['Line 1',
                           'Line 2',
                           'Line 3 ',
                           'Line 4 "foo"'],
                          clean_lines.lines)

        self.assertEquals(['Line 1',
                           'Line 2',
                           'Line 3 ',
                           'Line 4 ""'],
                          clean_lines.elided)

    def test_init_empty(self):
        clean_lines = cpp_style.CleansedLines([])
        self.assertEquals([], clean_lines.raw_lines)
        self.assertEquals(0, clean_lines.num_lines())

    def test_collapse_strings(self):
        collapse = cpp_style.CleansedLines.collapse_strings
        self.assertEquals('""', collapse('""'))             # ""     (empty)
        self.assertEquals('"""', collapse('"""'))           # """    (bad)
        self.assertEquals('""', collapse('"xyz"'))          # "xyz"  (string)
        self.assertEquals('""', collapse('"\\\""'))         # "\""   (string)
        self.assertEquals('""', collapse('"\'"'))           # "'"    (string)
        self.assertEquals('"\"', collapse('"\"'))           # "\"    (bad)
        self.assertEquals('""', collapse('"\\\\"'))         # "\\"   (string)
        self.assertEquals('"', collapse('"\\\\\\"'))        # "\\\"  (bad)
        self.assertEquals('""', collapse('"\\\\\\\\"'))     # "\\\\" (string)

        self.assertEquals('\'\'', collapse('\'\''))         # ''     (empty)
        self.assertEquals('\'\'', collapse('\'a\''))        # 'a'    (char)
        self.assertEquals('\'\'', collapse('\'\\\'\''))     # '\''   (char)
        self.assertEquals('\'', collapse('\'\\\''))         # '\'    (bad)
        self.assertEquals('', collapse('\\012'))            # '\012' (char)
        self.assertEquals('', collapse('\\xfF0'))           # '\xfF0' (char)
        self.assertEquals('', collapse('\\n'))              # '\n' (char)
        self.assertEquals('\#', collapse('\\#'))            # '\#' (bad)

        self.assertEquals('StringReplace(body, "", "");',
                          collapse('StringReplace(body, "\\\\", "\\\\\\\\");'))
        self.assertEquals('\'\' ""',
                          collapse('\'"\' "foo"'))


class OrderOfIncludesTest(CppStyleTestBase):
    def setUp(self):
        self.include_state = cpp_style._IncludeState()

        # Cheat os.path.abspath called in FileInfo class.
        self.os_path_abspath_orig = os.path.abspath
        os.path.abspath = lambda value: value

    def tearDown(self):
        os.path.abspath = self.os_path_abspath_orig

    def test_try_drop_common_suffixes(self):
        self.assertEqual('foo/foo', cpp_style._drop_common_suffixes('foo/foo-inl.h'))
        self.assertEqual('foo/bar/foo',
                         cpp_style._drop_common_suffixes('foo/bar/foo_inl.h'))
        self.assertEqual('foo/foo', cpp_style._drop_common_suffixes('foo/foo.cpp'))
        self.assertEqual('foo/foo_unusualinternal',
                         cpp_style._drop_common_suffixes('foo/foo_unusualinternal.h'))
        self.assertEqual('',
                         cpp_style._drop_common_suffixes('_test.cpp'))
        self.assertEqual('test',
                         cpp_style._drop_common_suffixes('test.cpp'))


class OrderOfIncludesTest(CppStyleTestBase):
    def setUp(self):
        self.include_state = cpp_style._IncludeState()

        # Cheat os.path.abspath called in FileInfo class.
        self.os_path_abspath_orig = os.path.abspath
        os.path.abspath = lambda value: value

    def tearDown(self):
        os.path.abspath = self.os_path_abspath_orig

    def test_check_next_include_order__no_config(self):
        self.assertEqual('Header file should not contain WebCore config.h.',
                         self.include_state.check_next_include_order(cpp_style._CONFIG_HEADER, True))

    def test_check_next_include_order__no_self(self):
        self.assertEqual('Header file should not contain itself.',
                         self.include_state.check_next_include_order(cpp_style._PRIMARY_HEADER, True))
        # Test actual code to make sure that header types are correctly assigned.
        self.assert_language_rules_check('Foo.h',
                                         '#include "Foo.h"\n',
                                         'Header file should not contain itself. Should be: alphabetically sorted.'
                                         '  [build/include_order] [4]')
        self.assert_language_rules_check('FooBar.h',
                                         '#include "Foo.h"\n',
                                         '')

    def test_check_next_include_order__likely_then_config(self):
        self.assertEqual('Found header this file implements before WebCore config.h.',
                         self.include_state.check_next_include_order(cpp_style._PRIMARY_HEADER, False))
        self.assertEqual('Found WebCore config.h after a header this file implements.',
                         self.include_state.check_next_include_order(cpp_style._CONFIG_HEADER, False))

    def test_check_next_include_order__other_then_config(self):
        self.assertEqual('Found other header before WebCore config.h.',
                         self.include_state.check_next_include_order(cpp_style._OTHER_HEADER, False))
        self.assertEqual('Found WebCore config.h after other header.',
                         self.include_state.check_next_include_order(cpp_style._CONFIG_HEADER, False))

    def test_check_next_include_order__config_then_other_then_likely(self):
        self.assertEqual('', self.include_state.check_next_include_order(cpp_style._CONFIG_HEADER, False))
        self.assertEqual('Found other header before a header this file implements.',
                         self.include_state.check_next_include_order(cpp_style._OTHER_HEADER, False))
        self.assertEqual('Found header this file implements after other header.',
                         self.include_state.check_next_include_order(cpp_style._PRIMARY_HEADER, False))

    def test_check_alphabetical_include_order(self):
        self.assert_language_rules_check('foo.h',
                                         '#include "a.h"\n'
                                         '#include "c.h"\n'
                                         '#include "b.h"\n',
                                         'Alphabetical sorting problem.  [build/include_order] [4]')

        self.assert_language_rules_check('foo.h',
                                         '#include "a.h"\n'
                                         '#include "b.h"\n'
                                         '#include "c.h"\n',
                                         '')

        self.assert_language_rules_check('foo.h',
                                         '#include <assert.h>\n'
                                         '#include "bar.h"\n',
                                         'Alphabetical sorting problem.  [build/include_order] [4]')

        self.assert_language_rules_check('foo.h',
                                         '#include "bar.h"\n'
                                         '#include <assert.h>\n',
                                         '')

    def test_check_line_break_after_own_header(self):
        self.assert_language_rules_check('foo.cpp',
                                         '#include "config.h"\n'
                                         '#include "foo.h"\n'
                                         '#include "bar.h"\n',
                                         'You should add a blank line after implementation file\'s own header.  [build/include_order] [4]')

        self.assert_language_rules_check('foo.cpp',
                                         '#include "config.h"\n'
                                         '#include "foo.h"\n'
                                         '\n'
                                         '#include "bar.h"\n',
                                         '')

    def test_check_preprocessor_in_include_section(self):
        self.assert_language_rules_check('foo.cpp',
                                         '#include "config.h"\n'
                                         '#include "foo.h"\n'
                                         '\n'
                                         '#ifdef BAZ\n'
                                         '#include "baz.h"\n'
                                         '#else\n'
                                         '#include "foobar.h"\n'
                                         '#endif"\n'
                                         '#include "bar.h"\n', # No flag because previous is in preprocessor section
                                         '')

        self.assert_language_rules_check('foo.cpp',
                                         '#include "config.h"\n'
                                         '#include "foo.h"\n'
                                         '\n'
                                         '#ifdef BAZ\n'
                                         '#include "baz.h"\n'
                                         '#endif"\n'
                                         '#include "bar.h"\n'
                                         '#include "a.h"\n', # Should still flag this.
                                         'Alphabetical sorting problem.  [build/include_order] [4]')

        self.assert_language_rules_check('foo.cpp',
                                         '#include "config.h"\n'
                                         '#include "foo.h"\n'
                                         '\n'
                                         '#ifdef BAZ\n'
                                         '#include "baz.h"\n'
                                         '#include "bar.h"\n' #Should still flag this
                                         '#endif"\n',
                                         'Alphabetical sorting problem.  [build/include_order] [4]')

        self.assert_language_rules_check('foo.cpp',
                                         '#include "config.h"\n'
                                         '#include "foo.h"\n'
                                         '\n'
                                         '#ifdef BAZ\n'
                                         '#include "baz.h"\n'
                                         '#endif"\n'
                                         '#ifdef FOOBAR\n'
                                         '#include "foobar.h"\n'
                                         '#endif"\n'
                                         '#include "bar.h"\n'
                                         '#include "a.h"\n', # Should still flag this.
                                         'Alphabetical sorting problem.  [build/include_order] [4]')

        # Check that after an already included error, the sorting rules still work.
        self.assert_language_rules_check('foo.cpp',
                                         '#include "config.h"\n'
                                         '#include "foo.h"\n'
                                         '\n'
                                         '#include "foo.h"\n'
                                         '#include "g.h"\n',
                                         '"foo.h" already included at foo.cpp:1  [build/include] [4]')

    def test_check_wtf_includes(self):
        self.assert_language_rules_check('foo.cpp',
                                         '#include "config.h"\n'
                                         '#include "foo.h"\n'
                                         '\n'
                                         '#include <wtf/Assertions.h>\n',
                                         '')
        self.assert_language_rules_check('foo.cpp',
                                         '#include "config.h"\n'
                                         '#include "foo.h"\n'
                                         '\n'
                                         '#include "wtf/Assertions.h"\n',
                                         'wtf includes should be <wtf/file.h> instead of "wtf/file.h".'
                                         '  [build/include] [4]')

    def test_classify_include(self):
        classify_include = cpp_style._classify_include
        include_state = cpp_style._IncludeState()
        self.assertEqual(cpp_style._CONFIG_HEADER,
                         classify_include('foo/foo.cpp',
                                          'config.h',
                                          False, include_state))
        self.assertEqual(cpp_style._PRIMARY_HEADER,
                         classify_include('foo/internal/foo.cpp',
                                          'foo/public/foo.h',
                                          False, include_state))
        self.assertEqual(cpp_style._PRIMARY_HEADER,
                         classify_include('foo/internal/foo.cpp',
                                          'foo/other/public/foo.h',
                                          False, include_state))
        self.assertEqual(cpp_style._OTHER_HEADER,
                         classify_include('foo/internal/foo.cpp',
                                          'foo/other/public/foop.h',
                                          False, include_state))
        self.assertEqual(cpp_style._OTHER_HEADER,
                         classify_include('foo/foo.cpp',
                                          'string',
                                          True, include_state))
        self.assertEqual(cpp_style._PRIMARY_HEADER,
                         classify_include('fooCustom.cpp',
                                          'foo.h',
                                          False, include_state))
        # Tricky example where both includes might be classified as primary.
        self.assert_language_rules_check('ScrollbarThemeWince.cpp',
                                         '#include "config.h"\n'
                                         '#include "ScrollbarThemeWince.h"\n'
                                         '\n'
                                         '#include "Scrollbar.h"\n',
                                         '')
        self.assert_language_rules_check('ScrollbarThemeWince.cpp',
                                         '#include "config.h"\n'
                                         '#include "Scrollbar.h"\n'
                                         '\n'
                                         '#include "ScrollbarThemeWince.h"\n',
                                         'Found header this file implements after a header this file implements.'
                                         ' Should be: config.h, primary header, blank line, and then alphabetically sorted.'
                                         '  [build/include_order] [4]')

    def test_try_drop_common_suffixes(self):
        self.assertEqual('foo/foo', cpp_style._drop_common_suffixes('foo/foo-inl.h'))
        self.assertEqual('foo/bar/foo',
                         cpp_style._drop_common_suffixes('foo/bar/foo_inl.h'))
        self.assertEqual('foo/foo', cpp_style._drop_common_suffixes('foo/foo.cpp'))
        self.assertEqual('foo/foo_unusualinternal',
                         cpp_style._drop_common_suffixes('foo/foo_unusualinternal.h'))
        self.assertEqual('',
                         cpp_style._drop_common_suffixes('_test.cpp'))
        self.assertEqual('test',
                         cpp_style._drop_common_suffixes('test.cpp'))
        self.assertEqual('test',
                         cpp_style._drop_common_suffixes('test.cpp'))

class CheckForFunctionLengthsTest(CppStyleTestBase):
    def setUp(self):
        # Reducing these thresholds for the tests speeds up tests significantly.
        self.old_normal_trigger = cpp_style._FunctionState._NORMAL_TRIGGER
        self.old_test_trigger = cpp_style._FunctionState._TEST_TRIGGER

        cpp_style._FunctionState._NORMAL_TRIGGER = 10
        cpp_style._FunctionState._TEST_TRIGGER = 25

    def tearDown(self):
        cpp_style._FunctionState._NORMAL_TRIGGER = self.old_normal_trigger
        cpp_style._FunctionState._TEST_TRIGGER = self.old_test_trigger

    def assert_function_lengths_check(self, code, expected_message):
        """Check warnings for long function bodies are as expected.

        Args:
          code: C++ source code expected to generate a warning message.
          expected_message: Message expected to be generated by the C++ code.
        """
        self.assertEquals(expected_message,
                          self.perform_function_lengths_check(code))

    def trigger_lines(self, error_level):
        """Return number of lines needed to trigger a function length warning.

        Args:
          error_level: --v setting for cpp_style.

        Returns:
          Number of lines needed to trigger a function length warning.
        """
        return cpp_style._FunctionState._NORMAL_TRIGGER * 2 ** error_level

    def trigger_test_lines(self, error_level):
        """Return number of lines needed to trigger a test function length warning.

        Args:
          error_level: --v setting for cpp_style.

        Returns:
          Number of lines needed to trigger a test function length warning.
        """
        return cpp_style._FunctionState._TEST_TRIGGER * 2 ** error_level

    def assert_function_length_check_definition(self, lines, error_level):
        """Generate long function definition and check warnings are as expected.

        Args:
          lines: Number of lines to generate.
          error_level:  --v setting for cpp_style.
        """
        trigger_level = self.trigger_lines(cpp_style._verbose_level())
        self.assert_function_lengths_check(
            'void test(int x)' + self.function_body(lines),
            ('Small and focused functions are preferred: '
             'test() has %d non-comment lines '
             '(error triggered by exceeding %d lines).'
             '  [readability/fn_size] [%d]'
             % (lines, trigger_level, error_level)))

    def assert_function_length_check_definition_ok(self, lines):
        """Generate shorter function definition and check no warning is produced.

        Args:
          lines: Number of lines to generate.
        """
        self.assert_function_lengths_check(
            'void test(int x)' + self.function_body(lines),
            '')

    def assert_function_length_check_at_error_level(self, error_level):
        """Generate and check function at the trigger level for --v setting.

        Args:
          error_level: --v setting for cpp_style.
        """
        self.assert_function_length_check_definition(self.trigger_lines(error_level),
                                                     error_level)

    def assert_function_length_check_below_error_level(self, error_level):
        """Generate and check function just below the trigger level for --v setting.

        Args:
          error_level: --v setting for cpp_style.
        """
        self.assert_function_length_check_definition(self.trigger_lines(error_level) - 1,
                                                     error_level - 1)

    def assert_function_length_check_above_error_level(self, error_level):
        """Generate and check function just above the trigger level for --v setting.

        Args:
          error_level: --v setting for cpp_style.
        """
        self.assert_function_length_check_definition(self.trigger_lines(error_level) + 1,
                                                     error_level)

    def function_body(self, number_of_lines):
        return ' {\n' + '    this_is_just_a_test();\n' * number_of_lines + '}'

    def function_body_with_blank_lines(self, number_of_lines):
        return ' {\n' + '    this_is_just_a_test();\n\n' * number_of_lines + '}'

    def function_body_with_no_lints(self, number_of_lines):
        return ' {\n' + '    this_is_just_a_test();  // NOLINT\n' * number_of_lines + '}'

    # Test line length checks.
    def test_function_length_check_declaration(self):
        self.assert_function_lengths_check(
            'void test();',  # Not a function definition
            '')

    def test_function_length_check_declaration_with_block_following(self):
        self.assert_function_lengths_check(
            ('void test();\n'
             + self.function_body(66)),  # Not a function definition
            '')

    def test_function_length_check_class_definition(self):
        self.assert_function_lengths_check(  # Not a function definition
            'class Test' + self.function_body(66) + ';',
            '')

    def test_function_length_check_trivial(self):
        self.assert_function_lengths_check(
            'void test() {}',  # Not counted
            '')

    def test_function_length_check_empty(self):
        self.assert_function_lengths_check(
            'void test() {\n}',
            '')

    def test_function_length_check_definition_below_severity0(self):
        old_verbosity = cpp_style._set_verbose_level(0)
        self.assert_function_length_check_definition_ok(self.trigger_lines(0) - 1)
        cpp_style._set_verbose_level(old_verbosity)

    def test_function_length_check_definition_at_severity0(self):
        old_verbosity = cpp_style._set_verbose_level(0)
        self.assert_function_length_check_definition_ok(self.trigger_lines(0))
        cpp_style._set_verbose_level(old_verbosity)

    def test_function_length_check_definition_above_severity0(self):
        old_verbosity = cpp_style._set_verbose_level(0)
        self.assert_function_length_check_above_error_level(0)
        cpp_style._set_verbose_level(old_verbosity)

    def test_function_length_check_definition_below_severity1v0(self):
        old_verbosity = cpp_style._set_verbose_level(0)
        self.assert_function_length_check_below_error_level(1)
        cpp_style._set_verbose_level(old_verbosity)

    def test_function_length_check_definition_at_severity1v0(self):
        old_verbosity = cpp_style._set_verbose_level(0)
        self.assert_function_length_check_at_error_level(1)
        cpp_style._set_verbose_level(old_verbosity)

    def test_function_length_check_definition_below_severity1(self):
        self.assert_function_length_check_definition_ok(self.trigger_lines(1) - 1)

    def test_function_length_check_definition_at_severity1(self):
        self.assert_function_length_check_definition_ok(self.trigger_lines(1))

    def test_function_length_check_definition_above_severity1(self):
        self.assert_function_length_check_above_error_level(1)

    def test_function_length_check_definition_severity1_plus_blanks(self):
        error_level = 1
        error_lines = self.trigger_lines(error_level) + 1
        trigger_level = self.trigger_lines(cpp_style._verbose_level())
        self.assert_function_lengths_check(
            'void test_blanks(int x)' + self.function_body(error_lines),
            ('Small and focused functions are preferred: '
             'test_blanks() has %d non-comment lines '
             '(error triggered by exceeding %d lines).'
             '  [readability/fn_size] [%d]')
            % (error_lines, trigger_level, error_level))

    def test_function_length_check_complex_definition_severity1(self):
        error_level = 1
        error_lines = self.trigger_lines(error_level) + 1
        trigger_level = self.trigger_lines(cpp_style._verbose_level())
        self.assert_function_lengths_check(
            ('my_namespace::my_other_namespace::MyVeryLongTypeName*\n'
             'my_namespace::my_other_namespace::MyFunction(int arg1, char* arg2)'
             + self.function_body(error_lines)),
            ('Small and focused functions are preferred: '
             'my_namespace::my_other_namespace::MyFunction()'
             ' has %d non-comment lines '
             '(error triggered by exceeding %d lines).'
             '  [readability/fn_size] [%d]')
            % (error_lines, trigger_level, error_level))

    def test_function_length_check_definition_severity1_for_test(self):
        error_level = 1
        error_lines = self.trigger_test_lines(error_level) + 1
        trigger_level = self.trigger_test_lines(cpp_style._verbose_level())
        self.assert_function_lengths_check(
            'TEST_F(Test, Mutator)' + self.function_body(error_lines),
            ('Small and focused functions are preferred: '
             'TEST_F(Test, Mutator) has %d non-comment lines '
             '(error triggered by exceeding %d lines).'
             '  [readability/fn_size] [%d]')
            % (error_lines, trigger_level, error_level))

    def test_function_length_check_definition_severity1_for_split_line_test(self):
        error_level = 1
        error_lines = self.trigger_test_lines(error_level) + 1
        trigger_level = self.trigger_test_lines(cpp_style._verbose_level())
        self.assert_function_lengths_check(
            ('TEST_F(GoogleUpdateRecoveryRegistryProtectedTest,\n'
             '    FixGoogleUpdate_AllValues_MachineApp)'  # note: 4 spaces
             + self.function_body(error_lines)),
            ('Small and focused functions are preferred: '
             'TEST_F(GoogleUpdateRecoveryRegistryProtectedTest, '  # 1 space
             'FixGoogleUpdate_AllValues_MachineApp) has %d non-comment lines '
             '(error triggered by exceeding %d lines).'
             '  [readability/fn_size] [%d]')
            % (error_lines+1, trigger_level, error_level))

    def test_function_length_check_definition_severity1_for_bad_test_doesnt_break(self):
        error_level = 1
        error_lines = self.trigger_test_lines(error_level) + 1
        trigger_level = self.trigger_test_lines(cpp_style._verbose_level())
        self.assert_function_lengths_check(
            ('TEST_F('
             + self.function_body(error_lines)),
            ('Small and focused functions are preferred: '
             'TEST_F has %d non-comment lines '
             '(error triggered by exceeding %d lines).'
             '  [readability/fn_size] [%d]')
            % (error_lines, trigger_level, error_level))

    def test_function_length_check_definition_severity1_with_embedded_no_lints(self):
        error_level = 1
        error_lines = self.trigger_lines(error_level) + 1
        trigger_level = self.trigger_lines(cpp_style._verbose_level())
        self.assert_function_lengths_check(
            'void test(int x)' + self.function_body_with_no_lints(error_lines),
            ('Small and focused functions are preferred: '
             'test() has %d non-comment lines '
             '(error triggered by exceeding %d lines).'
             '  [readability/fn_size] [%d]')
            % (error_lines, trigger_level, error_level))

    def test_function_length_check_definition_severity1_with_no_lint(self):
        self.assert_function_lengths_check(
            ('void test(int x)' + self.function_body(self.trigger_lines(1))
             + '  // NOLINT -- long function'),
            '')

    def test_function_length_check_definition_below_severity2(self):
        self.assert_function_length_check_below_error_level(2)

    def test_function_length_check_definition_severity2(self):
        self.assert_function_length_check_at_error_level(2)

    def test_function_length_check_definition_above_severity2(self):
        self.assert_function_length_check_above_error_level(2)

    def test_function_length_check_definition_below_severity3(self):
        self.assert_function_length_check_below_error_level(3)

    def test_function_length_check_definition_severity3(self):
        self.assert_function_length_check_at_error_level(3)

    def test_function_length_check_definition_above_severity3(self):
        self.assert_function_length_check_above_error_level(3)

    def test_function_length_check_definition_below_severity4(self):
        self.assert_function_length_check_below_error_level(4)

    def test_function_length_check_definition_severity4(self):
        self.assert_function_length_check_at_error_level(4)

    def test_function_length_check_definition_above_severity4(self):
        self.assert_function_length_check_above_error_level(4)

    def test_function_length_check_definition_below_severity5(self):
        self.assert_function_length_check_below_error_level(5)

    def test_function_length_check_definition_at_severity5(self):
        self.assert_function_length_check_at_error_level(5)

    def test_function_length_check_definition_above_severity5(self):
        self.assert_function_length_check_above_error_level(5)

    def test_function_length_check_definition_huge_lines(self):
        # 5 is the limit
        self.assert_function_length_check_definition(self.trigger_lines(10), 5)

    def test_function_length_not_determinable(self):
        # Macro invocation without terminating semicolon.
        self.assert_function_lengths_check(
            'MACRO(arg)',
            '')

        # Macro with underscores
        self.assert_function_lengths_check(
            'MACRO_WITH_UNDERSCORES(arg1, arg2, arg3)',
            '')

        self.assert_function_lengths_check(
            'NonMacro(arg)',
            'Lint failed to find start of function body.'
            '  [readability/fn_size] [5]')


class NoNonVirtualDestructorsTest(CppStyleTestBase):

    def test_no_error(self):
        self.assert_multi_line_lint(
            '''class Foo {
                   virtual ~Foo();
                   virtual void foo();
               };''',
            '')

        self.assert_multi_line_lint(
            '''class Foo {
                   virtual inline ~Foo();
                   virtual void foo();
               };''',
            '')

        self.assert_multi_line_lint(
            '''class Foo {
                   inline virtual ~Foo();
                   virtual void foo();
               };''',
            '')

        self.assert_multi_line_lint(
            '''class Foo::Goo {
                   virtual ~Goo();
                   virtual void goo();
               };''',
            '')
        self.assert_multi_line_lint(
            'class Foo { void foo(); };',
            'More than one command on the same line  [whitespace/newline] [4]')

        self.assert_multi_line_lint(
            '''class Qualified::Goo : public Foo {
                   virtual void goo();
               };''',
            '')

        self.assert_multi_line_lint(
            # Line-ending :
            '''class Goo :
               public Foo {
                    virtual void goo();
               };''',
            'Labels should always be indented at least one space.  If this is a '
            'member-initializer list in a constructor, the colon should be on the '
            'line after the definition header.  [whitespace/labels] [4]')

    def test_no_destructor_when_virtual_needed(self):
        self.assert_multi_line_lint_re(
            '''class Foo {
                   virtual void foo();
               };''',
            'The class Foo probably needs a virtual destructor')

    def test_destructor_non_virtual_when_virtual_needed(self):
        self.assert_multi_line_lint_re(
            '''class Foo {
                   ~Foo();
                   virtual void foo();
               };''',
            'The class Foo probably needs a virtual destructor')

    def test_no_warn_when_derived(self):
        self.assert_multi_line_lint(
            '''class Foo : public Goo {
                   virtual void foo();
               };''',
            '')

    def test_internal_braces(self):
        self.assert_multi_line_lint_re(
            '''class Foo {
                   enum Goo {
                       GOO
                   };
                   virtual void foo();
               };''',
            'The class Foo probably needs a virtual destructor')

    def test_inner_class_needs_virtual_destructor(self):
        self.assert_multi_line_lint_re(
            '''class Foo {
                   class Goo {
                       virtual void goo();
                   };
               };''',
            'The class Goo probably needs a virtual destructor')

    def test_outer_class_needs_virtual_destructor(self):
        self.assert_multi_line_lint_re(
            '''class Foo {
                   class Goo {
                   };
                   virtual void foo();
               };''',
            'The class Foo probably needs a virtual destructor')

    def test_qualified_class_needs_virtual_destructor(self):
        self.assert_multi_line_lint_re(
            '''class Qualified::Foo {
                   virtual void foo();
               };''',
            'The class Qualified::Foo probably needs a virtual destructor')

    def test_multi_line_declaration_no_error(self):
        self.assert_multi_line_lint_re(
            '''class Foo
                   : public Goo {
                   virtual void foo();
               };''',
            '')

    def test_multi_line_declaration_with_error(self):
        self.assert_multi_line_lint(
            '''class Foo
               {
                   virtual void foo();
               };''',
            ['This { should be at the end of the previous line  '
             '[whitespace/braces] [4]',
             'The class Foo probably needs a virtual destructor due to having '
             'virtual method(s), one declared at line 2.  [runtime/virtual] [4]'])


class CppStyleStateTest(unittest.TestCase):
    def test_error_count(self):
        self.assertEquals(0, cpp_style.error_count())
        cpp_style._cpp_style_state.increment_error_count()
        cpp_style._cpp_style_state.increment_error_count()
        self.assertEquals(2, cpp_style.error_count())
        cpp_style._cpp_style_state.reset_error_count()
        self.assertEquals(0, cpp_style.error_count())


class WebKitStyleTest(CppStyleTestBase):

    # for http://webkit.org/coding/coding-style.html
    def test_indentation(self):
        # 1. Use spaces, not tabs. Tabs should only appear in files that
        #    require them for semantic meaning, like Makefiles.
        self.assert_multi_line_lint(
            'class Foo {\n'
            '    int goo;\n'
            '};',
            '')
        self.assert_multi_line_lint(
            'class Foo {\n'
            '\tint goo;\n'
            '};',
            'Tab found; better to use spaces  [whitespace/tab] [1]')

        # 2. The indent size is 4 spaces.
        self.assert_multi_line_lint(
            'class Foo {\n'
            '    int goo;\n'
            '};',
            '')
        self.assert_multi_line_lint(
            'class Foo {\n'
            '   int goo;\n'
            '};',
            'Weird number of spaces at line-start.  Are you using a 4-space indent?  [whitespace/indent] [3]')
        # FIXME: No tests for 8-spaces.

        # 3. In a header, code inside a namespace should not be indented.
        self.assert_multi_line_lint(
            'namespace WebCore {\n\n'
            'class Document {\n'
            '    int myVariable;\n'
            '};\n'
            '}',
            '',
            'foo.h')
        self.assert_multi_line_lint(
            'namespace OuterNamespace {\n'
            '    namespace InnerNamespace {\n'
            '    class Document {\n'
            '};\n'
            '};\n'
            '}',
            ['Code inside a namespace should not be indented.  [whitespace/indent] [4]', 'namespace should never be indented.  [whitespace/indent] [4]'],
            'foo.h')
        self.assert_multi_line_lint(
            'namespace WebCore {\n'
            '#if 0\n'
            '    class Document {\n'
            '};\n'
            '#endif\n'
            '}',
            'Code inside a namespace should not be indented.  [whitespace/indent] [4]',
            'foo.h')
        self.assert_multi_line_lint(
            'namespace WebCore {\n'
            'class Document {\n'
            '};\n'
            '}',
            '',
            'foo.h')

        # 4. In an implementation file (files with the extension .cpp, .c
        #    or .mm), code inside a namespace should not be indented.
        self.assert_multi_line_lint(
            'namespace WebCore {\n\n'
            'Document::Foo()\n'
            '    : foo(bar)\n'
            '    , boo(far)\n'
            '{\n'
            '    stuff();\n'
            '}',
            '',
            'foo.cpp')
        self.assert_multi_line_lint(
            'namespace OuterNamespace {\n'
            'namespace InnerNamespace {\n'
            'Document::Foo() { }\n'
            '    void* p;\n'
            '}\n'
            '}\n',
            'Code inside a namespace should not be indented.  [whitespace/indent] [4]',
            'foo.cpp')
        self.assert_multi_line_lint(
            'namespace OuterNamespace {\n'
            'namespace InnerNamespace {\n'
            'Document::Foo() { }\n'
            '}\n'
            '    void* p;\n'
            '}\n',
            'Code inside a namespace should not be indented.  [whitespace/indent] [4]',
            'foo.cpp')
        self.assert_multi_line_lint(
            'namespace WebCore {\n\n'
            '    const char* foo = "start:;"\n'
            '        "dfsfsfs";\n'
            '}\n',
            'Code inside a namespace should not be indented.  [whitespace/indent] [4]',
            'foo.cpp')
        self.assert_multi_line_lint(
            'namespace WebCore {\n\n'
            'const char* foo(void* a = ";",  // ;\n'
            '    void* b);\n'
            '    void* p;\n'
            '}\n',
            'Code inside a namespace should not be indented.  [whitespace/indent] [4]',
            'foo.cpp')
        self.assert_multi_line_lint(
            'namespace WebCore {\n\n'
            'const char* foo[] = {\n'
            '    "void* b);",  // ;\n'
            '    "asfdf",\n'
            '    }\n'
            '    void* p;\n'
            '}\n',
            'Code inside a namespace should not be indented.  [whitespace/indent] [4]',
            'foo.cpp')
        self.assert_multi_line_lint(
            'namespace WebCore {\n\n'
            'const char* foo[] = {\n'
            '    "void* b);",  // }\n'
            '    "asfdf",\n'
            '    }\n'
            '}\n',
            '',
            'foo.cpp')
        self.assert_multi_line_lint(
            '    namespace WebCore {\n\n'
            '    void Document::Foo()\n'
            '    {\n'
            'start:  // infinite loops are fun!\n'
            '        goto start;\n'
            '    }',
            'namespace should never be indented.  [whitespace/indent] [4]',
            'foo.cpp')
        self.assert_multi_line_lint(
            'namespace WebCore {\n'
            '    Document::Foo() { }\n'
            '}',
            'Code inside a namespace should not be indented.'
            '  [whitespace/indent] [4]',
            'foo.cpp')
        self.assert_multi_line_lint(
            'namespace WebCore {\n'
            '#define abc(x) x; \\\n'
            '    x\n'
            '}',
            '',
            'foo.cpp')
        self.assert_multi_line_lint(
            'namespace WebCore {\n'
            '#define abc(x) x; \\\n'
            '    x\n'
            '    void* x;'
            '}',
            'Code inside a namespace should not be indented.  [whitespace/indent] [4]',
            'foo.cpp')

        # 5. A case label should line up with its switch statement. The
        #    case statement is indented.
        self.assert_multi_line_lint(
            '    switch (condition) {\n'
            '    case fooCondition:\n'
            '    case barCondition:\n'
            '        i++;\n'
            '        break;\n'
            '    default:\n'
            '        i--;\n'
            '    }\n',
            '')
        self.assert_multi_line_lint(
            '    switch (condition) {\n'
            '    case fooCondition:\n'
            '        switch (otherCondition) {\n'
            '        default:\n'
            '            return;\n'
            '        }\n'
            '    default:\n'
            '        i--;\n'
            '    }\n',
            '')
        self.assert_multi_line_lint(
            '    switch (condition) {\n'
            '    case fooCondition: break;\n'
            '    default: return;\n'
            '    }\n',
            '')
        self.assert_multi_line_lint(
            '    switch (condition) {\n'
            '        case fooCondition:\n'
            '        case barCondition:\n'
            '            i++;\n'
            '            break;\n'
            '        default:\n'
            '            i--;\n'
            '    }\n',
            'A case label should not be indented, but line up with its switch statement.'
            '  [whitespace/indent] [4]')
        self.assert_multi_line_lint(
            '    switch (condition) {\n'
            '        case fooCondition:\n'
            '            break;\n'
            '    default:\n'
            '            i--;\n'
            '    }\n',
            'A case label should not be indented, but line up with its switch statement.'
            '  [whitespace/indent] [4]')
        self.assert_multi_line_lint(
            '    switch (condition) {\n'
            '    case fooCondition:\n'
            '    case barCondition:\n'
            '        switch (otherCondition) {\n'
            '            default:\n'
            '            return;\n'
            '        }\n'
            '    default:\n'
            '        i--;\n'
            '    }\n',
            'A case label should not be indented, but line up with its switch statement.'
            '  [whitespace/indent] [4]')
        self.assert_multi_line_lint(
            '    switch (condition) {\n'
            '    case fooCondition:\n'
            '    case barCondition:\n'
            '    i++;\n'
            '    break;\n\n'
            '    default:\n'
            '    i--;\n'
            '    }\n',
            'Non-label code inside switch statements should be indented.'
            '  [whitespace/indent] [4]')
        self.assert_multi_line_lint(
            '    switch (condition) {\n'
            '    case fooCondition:\n'
            '    case barCondition:\n'
            '        switch (otherCondition) {\n'
            '        default:\n'
            '        return;\n'
            '        }\n'
            '    default:\n'
            '        i--;\n'
            '    }\n',
            'Non-label code inside switch statements should be indented.'
            '  [whitespace/indent] [4]')

        # 6. Boolean expressions at the same nesting level that span
        #   multiple lines should have their operators on the left side of
        #   the line instead of the right side.
        self.assert_multi_line_lint(
            '    return attr->name() == srcAttr\n'
            '        || attr->name() == lowsrcAttr;\n',
            '')
        self.assert_multi_line_lint(
            '    return attr->name() == srcAttr ||\n'
            '        attr->name() == lowsrcAttr;\n',
            'Boolean expressions that span multiple lines should have their '
            'operators on the left side of the line instead of the right side.'
            '  [whitespace/operators] [4]')

    def test_spacing(self):
        # 1. Do not place spaces around unary operators.
        self.assert_multi_line_lint(
            'i++;',
            '')
        self.assert_multi_line_lint(
            'i ++;',
            'Extra space for operator  ++;  [whitespace/operators] [4]')

        # 2. Do place spaces around binary and ternary operators.
        self.assert_multi_line_lint(
            'y = m * x + b;',
            '')
        self.assert_multi_line_lint(
            'f(a, b);',
            '')
        self.assert_multi_line_lint(
            'c = a | b;',
            '')
        self.assert_multi_line_lint(
            'return condition ? 1 : 0;',
            '')
        self.assert_multi_line_lint(
            'y=m*x+b;',
            'Missing spaces around =  [whitespace/operators] [4]')
        self.assert_multi_line_lint(
            'f(a,b);',
            'Missing space after ,  [whitespace/comma] [3]')
        self.assert_multi_line_lint(
            'c = a|b;',
            'Missing spaces around |  [whitespace/operators] [3]')
        # FIXME: We cannot catch this lint error.
        # self.assert_multi_line_lint(
        #     'return condition ? 1:0;',
        #     '')

        # 3. Place spaces between control statements and their parentheses.
        self.assert_multi_line_lint(
            '    if (condition)\n'
            '        doIt();\n',
            '')
        self.assert_multi_line_lint(
            '    if(condition)\n'
            '        doIt();\n',
            'Missing space before ( in if(  [whitespace/parens] [5]')

        # 4. Do not place spaces between a function and its parentheses,
        #    or between a parenthesis and its content.
        self.assert_multi_line_lint(
            'f(a, b);',
            '')
        self.assert_multi_line_lint(
            'f (a, b);',
            'Extra space before ( in function call  [whitespace/parens] [4]')
        self.assert_multi_line_lint(
            'f( a, b );',
            ['Extra space after ( in function call  [whitespace/parens] [4]',
             'Extra space before )  [whitespace/parens] [2]'])

    def test_line_breaking(self):
        # 1. Each statement should get its own line.
        self.assert_multi_line_lint(
            '    x++;\n'
            '    y++;\n'
            '    if (condition);\n'
            '        doIt();\n',
            '')
        self.assert_multi_line_lint(
            '    x++; y++;',
            'More than one command on the same line  [whitespace/newline] [4]')
        # FIXME: Make this fail.
        # self.assert_multi_line_lint(
        #     '    if (condition) doIt();\n',
        #     '')

        # 2. An else statement should go on the same line as a preceding
        #   close brace if one is present, else it should line up with the
        #   if statement.
        self.assert_multi_line_lint(
            'if (condition) {\n'
            '    doSomething();\n'
            '    doSomethingAgain();\n'
            '} else {\n'
            '    doSomethingElse();\n'
            '    doSomethingElseAgain();\n'
            '}\n',
            '')
        self.assert_multi_line_lint(
            'if (condition)\n'
            '    doSomething();\n'
            'else\n'
            '    doSomethingElse();\n',
            '')
        self.assert_multi_line_lint(
            'if (condition)\n'
            '    doSomething();\n'
            'else {\n'
            '    doSomethingElse();\n'
            '    doSomethingElseAgain();\n'
            '}\n',
            '')

        self.assert_multi_line_lint(
            'if (condition) {\n'
            '    doSomething();\n'
            '    doSomethingAgain();\n'
            '}\n'
            'else {\n'
            '    doSomethingElse();\n'
            '    doSomethingElseAgain();\n'
            '}\n',
            'An else should appear on the same line as the preceding }  [whitespace/newline] [4]')
        self.assert_multi_line_lint(
            'if (condition) doSomething(); else doSomethingElse();\n',
            ['More than one command on the same line  [whitespace/newline] [4]',
             'Else clause should never be on same line as else (use 2 lines)  [whitespace/newline] [4]'])
        # FIXME: Make this fail.
        # self.assert_multi_line_lint(
        #     'if (condition) doSomething(); else {\n'
        #     '    doSomethingElse();\n'
        #     '}\n',
        #     '')

        # 3. An else if statement should be written as an if statement
        #    when the prior if concludes with a return statement.
        self.assert_multi_line_lint(
            'if (motivated) {\n'
            '    if (liquid)\n'
            '        return money;\n'
            '} else if (tired)\n'
            '    break;\n',
            '')
        self.assert_multi_line_lint(
            'if (condition)\n'
            '    doSomething();\n'
            'else if (otherCondition)\n'
            '    doSomethingElse();\n',
            '')
        self.assert_multi_line_lint(
            'if (condition)\n'
            '    doSomething();\n'
            'else\n'
            '    doSomethingElse();\n',
            '')
        self.assert_multi_line_lint(
            'if (condition)\n'
            '    returnValue = foo;\n'
            'else if (otherCondition)\n'
            '    returnValue = bar;\n',
            '')
        self.assert_multi_line_lint(
            'if (condition)\n'
            '    returnValue = foo;\n'
            'else\n'
            '    returnValue = bar;\n',
            '')
        self.assert_multi_line_lint(
            'if (condition)\n'
            '    doSomething();\n'
            'else if (liquid)\n'
            '    return money;\n'
            'else if (broke)\n'
            '    return favor;\n'
            'else\n'
            '    sleep(28800);\n',
            '')
        self.assert_multi_line_lint(
            'if (liquid) {\n'
            '    prepare();\n'
            '    return money;\n'
            '} else if (greedy) {\n'
            '    keep();\n'
            '    return nothing;\n'
            '}\n',
            'An else if statement should be written as an if statement when the '
            'prior "if" concludes with a return, break, continue or goto statement.'
            '  [readability/control_flow] [4]')
        self.assert_multi_line_lint(
            '    if (stupid) {\n'
            'infiniteLoop:\n'
            '        goto infiniteLoop;\n'
            '    } else if (evil)\n'
            '        goto hell;\n',
            'An else if statement should be written as an if statement when the '
            'prior "if" concludes with a return, break, continue or goto statement.'
            '  [readability/control_flow] [4]')
        self.assert_multi_line_lint(
            'if (liquid)\n'
            '{\n'
            '    prepare();\n'
            '    return money;\n'
            '}\n'
            'else if (greedy)\n'
            '    keep();\n',
            ['This { should be at the end of the previous line  [whitespace/braces] [4]',
            'An else should appear on the same line as the preceding }  [whitespace/newline] [4]',
            'An else if statement should be written as an if statement when the '
            'prior "if" concludes with a return, break, continue or goto statement.'
            '  [readability/control_flow] [4]'])
        self.assert_multi_line_lint(
            'if (gone)\n'
            '    return;\n'
            'else if (here)\n'
            '    go();\n',
            'An else if statement should be written as an if statement when the '
            'prior "if" concludes with a return, break, continue or goto statement.'
            '  [readability/control_flow] [4]')
        self.assert_multi_line_lint(
            'if (gone)\n'
            '    return;\n'
            'else\n'
            '    go();\n',
            'An else statement can be removed when the prior "if" concludes '
            'with a return, break, continue or goto statement.'
            '  [readability/control_flow] [4]')
        self.assert_multi_line_lint(
            'if (motivated) {\n'
            '    prepare();\n'
            '    continue;\n'
            '} else {\n'
            '    cleanUp();\n'
            '    break;\n'
            '}\n',
            'An else statement can be removed when the prior "if" concludes '
            'with a return, break, continue or goto statement.'
            '  [readability/control_flow] [4]')
        self.assert_multi_line_lint(
            'if (tired)\n'
            '    break;\n'
            'else {\n'
            '    prepare();\n'
            '    continue;\n'
            '}\n',
            'An else statement can be removed when the prior "if" concludes '
            'with a return, break, continue or goto statement.'
            '  [readability/control_flow] [4]')

    def test_braces(self):
        # 1. Function definitions: place each brace on its own line.
        self.assert_multi_line_lint(
            'int main()\n'
            '{\n'
            '    doSomething();\n'
            '}\n',
            '')
        self.assert_multi_line_lint(
            'int main() {\n'
            '    doSomething();\n'
            '}\n',
            'Place brace on its own line for function definitions.  [whitespace/braces] [4]')

        # 2. Other braces: place the open brace on the line preceding the
        #    code block; place the close brace on its own line.
        self.assert_multi_line_lint(
            'class MyClass {\n'
            '    int foo;\n'
            '};\n',
            '')
        self.assert_multi_line_lint(
            'namespace WebCore {\n'
            'int foo;\n'
            '};\n',
            '')
        self.assert_multi_line_lint(
            'for (int i = 0; i < 10; i++) {\n'
            '    DoSomething();\n'
            '};\n',
            '')
        self.assert_multi_line_lint(
            'class MyClass\n'
            '{\n'
            '    int foo;\n'
            '};\n',
            'This { should be at the end of the previous line  [whitespace/braces] [4]')
        self.assert_multi_line_lint(
            'if (condition)\n'
            '{\n'
            '    int foo;\n'
            '}\n',
            'This { should be at the end of the previous line  [whitespace/braces] [4]')
        self.assert_multi_line_lint(
            'for (int i = 0; i < 10; i++)\n'
            '{\n'
            '    int foo;\n'
            '}\n',
            'This { should be at the end of the previous line  [whitespace/braces] [4]')
        self.assert_multi_line_lint(
            'while (true)\n'
            '{\n'
            '    int foo;\n'
            '}\n',
            'This { should be at the end of the previous line  [whitespace/braces] [4]')
        self.assert_multi_line_lint(
            'foreach (Foo* foo, foos)\n'
            '{\n'
            '    int bar;\n'
            '}\n',
            'This { should be at the end of the previous line  [whitespace/braces] [4]')
        self.assert_multi_line_lint(
            'switch (type)\n'
            '{\n'
            'case foo: return;\n'
            '}\n',
            'This { should be at the end of the previous line  [whitespace/braces] [4]')
        self.assert_multi_line_lint(
            'if (condition)\n'
            '{\n'
            '    int foo;\n'
            '}\n',
            'This { should be at the end of the previous line  [whitespace/braces] [4]')
        self.assert_multi_line_lint(
            'for (int i = 0; i < 10; i++)\n'
            '{\n'
            '    int foo;\n'
            '}\n',
            'This { should be at the end of the previous line  [whitespace/braces] [4]')
        self.assert_multi_line_lint(
            'while (true)\n'
            '{\n'
            '    int foo;\n'
            '}\n',
            'This { should be at the end of the previous line  [whitespace/braces] [4]')
        self.assert_multi_line_lint(
            'switch (type)\n'
            '{\n'
            'case foo: return;\n'
            '}\n',
            'This { should be at the end of the previous line  [whitespace/braces] [4]')
        self.assert_multi_line_lint(
            'else if (type)\n'
            '{\n'
            'case foo: return;\n'
            '}\n',
            'This { should be at the end of the previous line  [whitespace/braces] [4]')

        # 3. One-line control clauses should not use braces unless
        #    comments are included or a single statement spans multiple
        #    lines.
        self.assert_multi_line_lint(
            'if (true) {\n'
            '    int foo;\n'
            '}\n',
            'One line control clauses should not use braces.  [whitespace/braces] [4]')

        self.assert_multi_line_lint(
            'for (; foo; bar) {\n'
            '    int foo;\n'
            '}\n',
            'One line control clauses should not use braces.  [whitespace/braces] [4]')

        self.assert_multi_line_lint(
            'foreach (foo, foos) {\n'
            '    int bar;\n'
            '}\n',
            'One line control clauses should not use braces.  [whitespace/braces] [4]')

        self.assert_multi_line_lint(
            'while (true) {\n'
            '    int foo;\n'
            '}\n',
            'One line control clauses should not use braces.  [whitespace/braces] [4]')

        self.assert_multi_line_lint(
            'if (true)\n'
            '    int foo;\n'
            'else {\n'
            '    int foo;\n'
            '}\n',
            'One line control clauses should not use braces.  [whitespace/braces] [4]')

        self.assert_multi_line_lint(
            'if (true) {\n'
            '    int foo;\n'
            '} else\n'
            '    int foo;\n',
            'One line control clauses should not use braces.  [whitespace/braces] [4]')

        self.assert_multi_line_lint(
            'if (true) {\n'
            '    // Some comment\n'
            '    int foo;\n'
            '}\n',
            '')

        self.assert_multi_line_lint(
            'if (true) {\n'
            '    myFunction(reallyLongParam1, reallyLongParam2,\n'
            '               reallyLongParam3);\n'
            '}\n',
            '')

        # 4. Control clauses without a body should use empty braces.
        self.assert_multi_line_lint(
            'for ( ; current; current = current->next) { }\n',
            '')
        self.assert_multi_line_lint(
            'for ( ; current;\n'
            '     current = current->next) {}\n',
            '')
        self.assert_multi_line_lint(
            'for ( ; current; current = current->next);\n',
            'Semicolon defining empty statement for this loop. Use { } instead.  [whitespace/semicolon] [5]')
        self.assert_multi_line_lint(
            'while (true);\n',
            'Semicolon defining empty statement for this loop. Use { } instead.  [whitespace/semicolon] [5]')
        self.assert_multi_line_lint(
            '} while (true);\n',
            '')

    def test_null_false_zero(self):
        # 1. In C++, the null pointer value should be written as 0. In C,
        #    it should be written as NULL. In Objective-C and Objective-C++,
        #    follow the guideline for C or C++, respectively, but use nil to
        #    represent a null Objective-C object.
        self.assert_lint(
            'functionCall(NULL)',
            'Use 0 instead of NULL.'
            '  [readability/null] [5]',
            'foo.cpp')
        self.assert_lint(
            "// Don't use NULL in comments since it isn't in code.",
            'Use 0 instead of NULL.'
            '  [readability/null] [4]',
            'foo.cpp')
        self.assert_lint(
            '"A string with NULL"  // and a comment with NULL is tricky to flag correctly in cpp_style.',
            'Use 0 instead of NULL.'
            '  [readability/null] [4]',
            'foo.cpp')
        self.assert_lint(
            '"A string containing NULL is ok"',
            '',
            'foo.cpp')
        self.assert_lint(
            'if (aboutNULL)',
            '',
            'foo.cpp')
        self.assert_lint(
            'myVariable = NULLify',
            '',
            'foo.cpp')
        # Make sure that the NULL check does not apply to C and Objective-C files.
        self.assert_lint(
            'functionCall(NULL)',
            '',
            'foo.c')
        self.assert_lint(
            'functionCall(NULL)',
            '',
            'foo.m')

        # 2. C++ and C bool values should be written as true and
        #    false. Objective-C BOOL values should be written as YES and NO.
        # FIXME: Implement this.

        # 3. Tests for true/false, null/non-null, and zero/non-zero should
        #    all be done without equality comparisons.
        self.assert_lint(
            'if (count == 0)',
            'Tests for true/false, null/non-null, and zero/non-zero should all be done without equality comparisons.'
            '  [readability/comparison_to_zero] [5]')
        self.assert_lint_one_of_many_errors_re(
            'if (string != NULL)',
            r'Tests for true/false, null/non-null, and zero/non-zero should all be done without equality comparisons\.')
        self.assert_lint(
            'if (condition == true)',
            'Tests for true/false, null/non-null, and zero/non-zero should all be done without equality comparisons.'
            '  [readability/comparison_to_zero] [5]')
        self.assert_lint(
            'if (myVariable != /* Why would anyone put a comment here? */ false)',
            'Tests for true/false, null/non-null, and zero/non-zero should all be done without equality comparisons.'
            '  [readability/comparison_to_zero] [5]')

        self.assert_lint(
            'if (0 /* This comment also looks odd to me. */ != aLongerVariableName)',
            'Tests for true/false, null/non-null, and zero/non-zero should all be done without equality comparisons.'
            '  [readability/comparison_to_zero] [5]')
        self.assert_lint_one_of_many_errors_re(
            'if (NULL == thisMayBeNull)',
            r'Tests for true/false, null/non-null, and zero/non-zero should all be done without equality comparisons\.')
        self.assert_lint(
            'if (true != anotherCondition)',
            'Tests for true/false, null/non-null, and zero/non-zero should all be done without equality comparisons.'
            '  [readability/comparison_to_zero] [5]')
        self.assert_lint(
            'if (false == myBoolValue)',
            'Tests for true/false, null/non-null, and zero/non-zero should all be done without equality comparisons.'
            '  [readability/comparison_to_zero] [5]')

        self.assert_lint(
            'if (fontType == trueType)',
            '')
        self.assert_lint(
            'if (othertrue == fontType)',
            '')

    def test_using_std(self):
        self.assert_lint(
            'using std::min;',
            "Use 'using namespace std;' instead of 'using std::min;'."
            "  [build/using_std] [4]",
            'foo.cpp')

    def test_max_macro(self):
        self.assert_lint(
            'int i = MAX(0, 1);',
            '',
            'foo.c')

        self.assert_lint(
            'int i = MAX(0, 1);',
            'Use std::max() or std::max<type>() instead of the MAX() macro.'
            '  [runtime/max_min_macros] [4]',
            'foo.cpp')

        self.assert_lint(
            'inline int foo() { return MAX(0, 1); }',
            'Use std::max() or std::max<type>() instead of the MAX() macro.'
            '  [runtime/max_min_macros] [4]',
            'foo.h')

    def test_min_macro(self):
        self.assert_lint(
            'int i = MIN(0, 1);',
            '',
            'foo.c')

        self.assert_lint(
            'int i = MIN(0, 1);',
            'Use std::min() or std::min<type>() instead of the MIN() macro.'
            '  [runtime/max_min_macros] [4]',
            'foo.cpp')

        self.assert_lint(
            'inline int foo() { return MIN(0, 1); }',
            'Use std::min() or std::min<type>() instead of the MIN() macro.'
            '  [runtime/max_min_macros] [4]',
            'foo.h')

    def test_names(self):
        # FIXME: Implement this.
        pass

    def test_other(self):
        # FIXME: Implement this.
        pass


def tearDown():
    """A global check to make sure all error-categories have been tested.

    The main tearDown() routine is the only code we can guarantee will be
    run after all other tests have been executed.
    """
    try:
        if _run_verifyallcategoriesseen:
            ErrorCollector(None).verify_all_categories_are_seen()
    except NameError:
        # If nobody set the global _run_verifyallcategoriesseen, then
        # we assume we shouldn't run the test
        pass

if __name__ == '__main__':
    import sys
    # We don't want to run the verify_all_categories_are_seen() test unless
    # we're running the full test suite: if we only run one test,
    # obviously we're not going to see all the error categories.  So we
    # only run verify_all_categories_are_seen() when no commandline flags
    # are passed in.
    global _run_verifyallcategoriesseen
    _run_verifyallcategoriesseen = (len(sys.argv) == 1)

    unittest.main()
