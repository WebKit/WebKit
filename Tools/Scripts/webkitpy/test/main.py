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

"""Contains the entry method for test-webkitpy."""

import logging
import os
import sys
import unittest

import webkitpy


_log = logging.getLogger(__name__)


class Tester(object):

    """Discovers and runs webkitpy unit tests."""

    def _find_unittest_files(self, webkitpy_dir):
        """Return a list of paths to all unit-test files."""
        unittest_paths = []  # Return value.

        for dir_path, dir_names, file_names in os.walk(webkitpy_dir):
            for file_name in file_names:
                if not file_name.endswith("_unittest.py"):
                    continue
                unittest_path = os.path.join(dir_path, file_name)
                unittest_paths.append(unittest_path)

        return unittest_paths

    def _modules_from_paths(self, package_root, paths):
        """Return a list of fully-qualified module names given paths."""
        package_path = os.path.abspath(package_root)
        root_package_name = os.path.split(package_path)[1]  # Equals "webkitpy".

        prefix_length = len(package_path)

        modules = []
        for path in paths:
            path = os.path.abspath(path)
            # This gives us, for example: /common/config/ports_unittest.py
            rel_path = path[prefix_length:]
            # This gives us, for example: /common/config/ports_unittest
            rel_path = os.path.splitext(rel_path)[0]

            parts = []
            while True:
                (rel_path, tail) = os.path.split(rel_path)
                if not tail:
                    break
                parts.insert(0, tail)
            # We now have, for example: common.config.ports_unittest
            # FIXME: This is all a hack around the fact that we always prefix webkitpy includes with "webkitpy."
            parts.insert(0, root_package_name)  # Put "webkitpy" at the beginning.
            module = ".".join(parts)
            modules.append(module)

        return modules

    def _win32_blacklist(self, module_path):
        # FIXME: Remove this once https://bugs.webkit.org/show_bug.cgi?id=54526 is resolved.
        if any([module_path.startswith(package) for package in [
            'webkitpy.tool',
            'webkitpy.common.checkout',
            'webkitpy.common.config',
            ]]):
            return False

        return module_path not in [
            # FIXME: This file also requires common.checkout to work
            'webkitpy.layout_tests.deduplicate_tests_unittest',
        ]

    def run_tests(self, sys_argv, external_package_paths=None):
        """Run the unit tests in all *_unittest.py modules in webkitpy.

        This method excludes "webkitpy.common.checkout.scm_unittest" unless
        the --all option is the second element of sys_argv.

        Args:
          sys_argv: A reference to sys.argv.

        """
        if external_package_paths is None:
            external_package_paths = []
        else:
            # FIXME: We should consider moving webkitpy off of using "webkitpy." to prefix
            # all includes.  If we did that, then this would use path instead of dirname(path).
            # QueueStatusServer.__init__ has a sys.path import hack due to this code.
            sys.path.extend(set(os.path.dirname(path) for path in external_package_paths))

        if len(sys_argv) > 1 and not sys_argv[-1].startswith("-"):
            # Then explicit modules or test names were provided, which
            # the unittest module is equipped to handle.
            unittest.main(argv=sys_argv, module=None)
            # No need to return since unitttest.main() exits.

        # Otherwise, auto-detect all unit tests.

        # FIXME: This should be combined with the external_package_paths code above.
        webkitpy_dir = os.path.dirname(webkitpy.__file__)

        modules = []
        for path in [webkitpy_dir] + external_package_paths:
            modules.extend(self._modules_from_paths(path, self._find_unittest_files(path)))
        modules.sort()

        # This is a sanity check to ensure that the unit-test discovery
        # methods are working.
        if len(modules) < 1:
            raise Exception("No unit-test modules found.")

        for module in modules:
            _log.debug("Found: %s" % module)

        # FIXME: This is a hack, but I'm tired of commenting out the test.
        #        See https://bugs.webkit.org/show_bug.cgi?id=31818
        if len(sys_argv) > 1 and sys.argv[1] == "--all":
            sys.argv.remove("--all")
        else:
            excluded_module = "webkitpy.common.checkout.scm_unittest"
            _log.info("Excluding: %s (use --all to include)" % excluded_module)
            modules.remove(excluded_module)

        if sys.platform == 'win32':
            modules = filter(self._win32_blacklist, modules)

        sys_argv.extend(modules)

        # We pass None for the module because we do not want the unittest
        # module to resolve module names relative to a given module.
        # (This would require importing all of the unittest modules from
        # this module.)  See the loadTestsFromName() method of the
        # unittest.TestLoader class for more details on this parameter.
        unittest.main(argv=sys_argv, module=None)
