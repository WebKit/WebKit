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

"""unit testing code for webkitpy."""

import logging
import os
import sys
import unittest

# NOTE: We intentionally do not depend on anything else in webkitpy here to avoid breaking test-webkitpy.

_log = logging.getLogger(__name__)


class Tester(object):
    """webkitpy unit tests driver (finds and runs tests)."""

    def init(self, command_args):
        """Execute code prior to importing from webkitpy.unittests.

        Args:
            command_args: The list of command-line arguments -- usually
                        sys.argv[1:].

        """
        verbose_logging_flag = "--verbose-logging"
        is_verbose_logging = verbose_logging_flag in command_args
        if is_verbose_logging:
            # Remove the flag so it doesn't cause unittest.main() to error out.
            #
            # FIXME: Get documentation for the --verbose-logging flag to show
            #        up in the usage instructions, which are currently generated
            #        by unittest.main().  It's possible that this will require
            #        re-implementing the option parser for unittest.main()
            #        since there may not be an easy way to modify its existing
            #        option parser.
            sys.argv.remove(verbose_logging_flag)

        if is_verbose_logging:
            self.configure_logging(logging.DEBUG)
            _log.debug("Verbose WebKit logging enabled.")
        else:
            self.configure_logging(logging.INFO)

    # Verbose logging is useful for debugging test-webkitpy code that runs
    # before the actual unit tests -- things like autoinstall downloading and
    # unit-test auto-detection logic.  This is different from verbose logging
    # of the unit tests themselves (i.e. the unittest module's --verbose flag).
    def configure_logging(self, log_level):
        """Configure the root logger.

        Configure the root logger not to log any messages from webkitpy --
        except for messages from the autoinstall module.  Also set the
        logging level as described below.
        """
        handler = logging.StreamHandler(sys.stderr)
        # We constrain the level on the handler rather than on the root
        # logger itself.  This is probably better because the handler is
        # configured and known only to this module, whereas the root logger
        # is an object shared (and potentially modified) by many modules.
        # Modifying the handler, then, is less intrusive and less likely to
        # interfere with modifications made by other modules (e.g. in unit
        # tests).
        handler.setLevel(log_level)
        formatter = logging.Formatter("%(message)s")
        handler.setFormatter(formatter)

        logger = logging.getLogger()
        logger.addHandler(handler)
        logger.setLevel(logging.NOTSET)

        # Filter out most webkitpy messages.
        #
        # Messages can be selectively re-enabled for this script by updating
        # this method accordingly.
        def filter(record):
            """Filter out autoinstall and non-third-party webkitpy messages."""
            # FIXME: Figure out a way not to use strings here, for example by
            #        using syntax like webkitpy.test.__name__.  We want to be
            #        sure not to import any non-Python 2.4 code, though, until
            #        after the version-checking code has executed.
            if (record.name.startswith("webkitpy.common.system.autoinstall") or
                record.name.startswith("webkitpy.test")):
                return True
            if record.name.startswith("webkitpy"):
                return False
            return True

        testing_filter = logging.Filter()
        testing_filter.filter = filter

        # Display a message so developers are not mystified as to why
        # logging does not work in the unit tests.
        _log.info("Suppressing most webkitpy logging while running unit tests.")
        handler.addFilter(testing_filter)

    def clean_packages(self, dirs):
        """Delete all .pyc files under dirs that have no .py file."""
        # We clean orphaned *.pyc files from the packages prior to importing from
        # them to make sure that no import statements falsely succeed.
        # This helps to check that import statements have been updated correctly
        # after any file moves.  Otherwise, incorrect import statements can
        # be masked.
        #
        # For example, if webkitpy/common/host.py were moved to a
        # different location without changing any import statements, and if
        # the corresponding .pyc file were left behind without deleting it,
        # then "import webkitpy.common.host" would continue to succeed
        # even though it would fail for someone checking out a fresh copy
        # of the source tree.  This is because of a Python feature:
        #
        # "It is possible to have a file called spam.pyc (or spam.pyo when -O
        # is used) without a file spam.py for the same module. This can be used
        # to distribute a library of Python code in a form that is moderately
        # hard to reverse engineer."
        #
        # ( http://docs.python.org/tutorial/modules.html#compiled-python-files )
        #
        # Deleting the orphaned .pyc file prior to importing, however, would
        # cause an ImportError to occur on import as desired.
        for dir_to_clean in dirs:
            _log.debug("Cleaning orphaned *.pyc files from: %s" % dir_to_clean)
            for dir_path, dir_names, file_names in os.walk(dir_to_clean):
                for file_name in file_names:
                    if file_name.endswith(".pyc") and file_name[:-1] not in file_names:
                        file_path = os.path.join(dir_path, file_name)
                        _log.info("Deleting orphan *.pyc file: %s" % file_path)
                        os.remove(file_path)

    def _find_under(self, dir_to_search, suffix):
        """Return a list of paths to all files under dir_to_search ending in suffix."""
        paths = []
        for dir_path, dir_names, file_names in os.walk(dir_to_search):
            for file_name in file_names:
                if file_name.endswith(suffix):
                    paths.append(os.path.join(dir_path, file_name))
        return paths

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
            'webkitpy.to_be_moved.deduplicate_tests_unittest',
        ]

    def run_tests(self, argv, dirs):
        """Run all the tests found under dirs."""
        # FIXME: We should consider moving webkitpy off of using "webkitpy." to prefix
        # all includes.  If we did that, then this would use path instead of dirname(path).
        # QueueStatusServer.__init__ has a sys.path import hack due to this code.
        sys.path.extend(set(os.path.dirname(path) for path in dirs))

        if '--xml' in argv:
            argv.remove('--xml')
            from webkitpy.thirdparty.autoinstalled.xmlrunner import XMLTestRunner
            test_runner = XMLTestRunner(output='test-webkitpy-xml-reports')
        else:
            test_runner = unittest.TextTestRunner

        if len(argv) > 1 and not argv[-1].startswith("-"):
            # Then explicit modules or test names were provided, which
            # the unittest module is equipped to handle.
            unittest.main(argv=argv, module=None, testRunner=test_runner)
            # No need to return since unitttest.main() exits.

        # Otherwise, auto-detect all unit tests.

        skip_integration_tests = False
        if len(argv) > 1 and argv[1] == "--skip-integrationtests":
            argv.remove("--skip-integrationtests")
            skip_integration_tests = True

        modules = []
        for dir_to_search in dirs:
            modules.extend(self._modules_from_paths(dir_to_search, self._find_under(dir_to_search, "_unittest.py")))
            if not skip_integration_tests:
                modules.extend(self._modules_from_paths(dir_to_search, self._find_under(dir_to_search, "_integrationtest.py")))
        modules.sort()

        # This is a sanity check to ensure that the unit-test discovery methods are working.
        if len(modules) < 1:
            raise Exception("No unit-test modules found.")

        for module in modules:
            _log.debug("Found: %s" % module)

        # FIXME: This is a hack, but I'm tired of commenting out the test.
        #        See https://bugs.webkit.org/show_bug.cgi?id=31818
        if len(argv) > 1 and argv[1] == "--all":
            argv.remove("--all")
        else:
            excluded_module = "webkitpy.common.checkout.scm.scm_unittest"
            _log.info("Excluding: %s (use --all to include)" % excluded_module)
            modules.remove(excluded_module)

        if sys.platform == 'win32':
            modules = filter(self._win32_blacklist, modules)

        # unittest.main has horrible error reporting when module imports are bad
        # so we test import here to make debugging bad imports much easier.
        for module in modules:
            __import__(module)

        argv.extend(modules)

        # We pass None for the module because we do not want the unittest
        # module to resolve module names relative to a given module.
        # (This would require importing all of the unittest modules from
        # this module.)  See the loadTestsFromName() method of the
        # unittest.TestLoader class for more details on this parameter.
        unittest.main(argv=argv, module=None, testRunner=test_runner)
