# Copyright (C) 2023 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
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

import os
import sys
import unittest

from webkitcorepy import AutoInstall, string_utils
from webkitcorepy.testing.test_runner import TestRunner


class PythonTestRunner(TestRunner):
    @classmethod
    def recurse(cls, suite):
        for element in suite:
            if isinstance(element, unittest.TestSuite):
                for test in cls.recurse(element):
                    yield test
            else:
                yield element

    def __init__(self, description, loggers=None, modules=None, patterns=None):
        super(PythonTestRunner, self).__init__(description, loggers=loggers)

        self._tests = {}
        if not modules:
            sys.stderr.write('No modules to test specified\n')
            return
        if not isinstance(modules, dict):
            sys.stderr.write('Modules must be specified as a dictionary in the form\n')
            sys.stderr.write('{\n')
            sys.stderr.write('    <root-path>: [<directory>, <direcotry>]\n')
            sys.stderr.write('}\n')
            return

        for root, dirs in modules.items():
            root = os.path.abspath(root)
            for d in dirs:
                path = os.path.join(root, d)
                if not os.path.isdir(path):
                    sys.stderr.write("'{}' does not exist\n".format(path))
                for pattern in patterns or ['*_unittest.py']:
                    for test in self.recurse(unittest.defaultTestLoader.discover(path, pattern=pattern, top_level_dir=root)):
                        self._tests[test.id()] = test

    def tests(self, args=None):
        filters = [] if not args or not args.tests else args.tests
        matched_filters = {pattern.pattern: 0 for pattern in filters}
        example = None
        for name in self._tests.keys():
            example = name
            if not filters:
                yield name
            did_match = False
            for pattern in filters:
                if not pattern.match(name):
                    continue
                if not did_match:
                    yield name
                did_match = True
                matched_filters[pattern.pattern] += 1

        must_exit = False
        for pattern, count in matched_filters.items():
            if count:
                continue
            sys.stderr.write("No tests match '{}'\n".format(pattern.replace('.*', '*').replace('**', '*')))
            sys.stderr.write("Specify tests names in the form '<module>.<file>.<class>.<function>'\n")
            if example:
                sys.stderr.write("For example: '{} {}'\n".format(sys.argv[0].split('/')[-1], example.split('.')[0]))
            must_exit = True
        if must_exit:
            sys.exit(-1)

    def run_test(self, test):
        result = unittest.TestResult()
        try:
            to_run = self._tests[test]
            to_run.run(result=result)
        except KeyError:
            result.errors.append((test, "No test named '{}'\n".format(test)))
        return result

    def id(self, test):
        if isinstance(test, string_utils.basestring):
            return test
        return test.id()

    def run(self, args):
        if AutoInstall.enabled():
            AutoInstall.install_everything()
        return super(PythonTestRunner, self).run(args)
