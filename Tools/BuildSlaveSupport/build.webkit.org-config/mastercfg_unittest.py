#! /usr/bin/python

import sys
import os
import StringIO
import unittest


class BuildBotConfigLoader(object):
    def _add_webkitpy_to_sys_path(self):
        # When files are passed to the python interpreter on the command line (e.g. python test.py) __file__ is a relative path.
        absolute_file_path = os.path.abspath(__file__)
        webkit_org_config_dir = os.path.dirname(absolute_file_path)
        build_slave_support_dir = os.path.dirname(webkit_org_config_dir)
        webkit_tools_dir = os.path.dirname(build_slave_support_dir)
        scripts_dir = os.path.join(webkit_tools_dir, 'Scripts')
        sys.path.append(scripts_dir)

    def _create_mock_passwords_dict(self):
        config_dict = simplejson.load(open('config.json'))
        return dict([(slave['name'], '1234') for slave in config_dict['slaves']])

    def _mock_open(self, filename):
        if filename == 'passwords.json':
            # FIXME: This depends on _add_dependant_modules_to_sys_modules imported simplejson.
            return StringIO.StringIO(simplejson.dumps(self._create_mock_passwords_dict()))
        return __builtins__.open(filename)

    def _add_dependant_modules_to_sys_modules(self):
        from webkitpy.thirdparty.autoinstalled import buildbot
        from webkitpy.thirdparty import simplejson

        sys.modules['buildbot'] = buildbot
        sys.modules['simplejson'] = simplejson

    def load_config(self, master_cfg_path):
        # Before we can use webkitpy.thirdparty, we need to fix our path to include webkitpy.
        # FIXME: If we're ever run by test-webkitpy we won't need this step.
        self._add_webkitpy_to_sys_path()
        # master.cfg expects the buildbot and simplejson modules to be in sys.path.
        self._add_dependant_modules_to_sys_modules()

        # master.cfg expects a passwords.json file which is not checked in.  Fake it by mocking open().
        globals()['open'] = self._mock_open
        # Because the master_cfg_path may have '.' in its name, we can't just use import, we have to use execfile.
        # We pass globals() as both the globals and locals to mimic exectuting in the global scope, so
        # that globals defined in master.cfg will be global to this file too.
        execfile(master_cfg_path, globals(), globals())
        globals()['open'] = __builtins__.open  # Stop mocking open().


class MasterCfgTest(unittest.TestCase):
    def test_nrwt_leaks_parsing(self):
        run_webkit_tests = RunWebKitTests()
        log_text = """
2011-08-09 10:05:18,580 29486 mac.py:275 INFO leaks found for a total of 197,936 bytes!
2011-08-09 10:05:18,580 29486 mac.py:276 INFO 1 unique leaks found!
"""
        expected_incorrect_lines = [
            'leaks found for a total of 197,936 bytes!',
            '1 unique leaks found!',
        ]
        run_webkit_tests._parseNewRunWebKitTestsOutput(log_text)
        self.assertEquals(run_webkit_tests.incorrectLayoutLines, expected_incorrect_lines)


class StubStdio(object):
    def __init__(self, stdio):
        self._stdio = stdio

    def getText(self):
        return self._stdio


class StubRemoteCommand(object):
    def __init__(self, rc, stdio):
        self.rc = rc
        self.logs = {'stdio': StubStdio(stdio)}


class RunUnitTestsTest(unittest.TestCase):
    def assertFailures(self, expected_failure_count, stdio):
        if expected_failure_count:
            rc = 1
            expected_results = FAILURE
            expected_text = '{0} unit tests failed or timed out'.format(expected_failure_count)
        else:
            rc = 0
            expected_results = SUCCESS
            expected_text = 'run-api-tests'

        cmd = StubRemoteCommand(rc, stdio)
        step = RunUnitTests()
        step.commandComplete(cmd)
        actual_results = step.evaluateCommand(cmd)
        actual_failure_count = step.failedTestCount
        actual_text = step.getText(cmd, actual_results)[0]

        self.assertEqual(expected_results, actual_results)
        self.assertEqual(expected_failure_count, actual_failure_count)
        self.assertEqual(expected_text, actual_text)

    def test_no_failures_or_timeouts(self):
        self.assertFailures(0, """Note: Google Test filter = WebViewDestructionWithHostWindow.DestroyViewWindowWithoutClose
[==========] Running 1 test from 1 test case.
[----------] Global test environment set-up.
[----------] 1 test from WebViewDestructionWithHostWindow
[ RUN      ] WebViewDestructionWithHostWindow.DestroyViewWindowWithoutClose
[       OK ] WebViewDestructionWithHostWindow.DestroyViewWindowWithoutClose (127 ms)
[----------] 1 test from WebViewDestructionWithHostWindow (127 ms total)

[----------] Global test environment tear-down
[==========] 1 test from 1 test case ran. (127 ms total)
[  PASSED  ] 1 test.
""")

    def test_one_failure(self):
        self.assertFailures(1, """Note: Google Test filter = WebViewDestructionWithHostWindow.DestroyViewWindowWithoutClose
[==========] Running 1 test from 1 test case.
[----------] Global test environment set-up.
[----------] 1 test from WebViewDestructionWithHostWindow
[ RUN      ] WebViewDestructionWithHostWindow.DestroyViewWindowWithoutClose
[       OK ] WebViewDestructionWithHostWindow.DestroyViewWindowWithoutClose (127 ms)
[----------] 1 test from WebViewDestructionWithHostWindow (127 ms total)

[----------] Global test environment tear-down
[==========] 1 test from 1 test case ran. (127 ms total)
[  PASSED  ] 1 test.
Tests that failed:
  WebKit2.WebKit2.CanHandleRequest
""")

    def test_multiple_failures(self):
        self.assertFailures(4, """Note: Google Test filter = WebViewDestructionWithHostWindow.DestroyViewWindowWithoutClose
[==========] Running 1 test from 1 test case.
[----------] Global test environment set-up.
[----------] 1 test from WebViewDestructionWithHostWindow
[ RUN      ] WebViewDestructionWithHostWindow.DestroyViewWindowWithoutClose
[       OK ] WebViewDestructionWithHostWindow.DestroyViewWindowWithoutClose (127 ms)
[----------] 1 test from WebViewDestructionWithHostWindow (127 ms total)

[----------] Global test environment tear-down
[==========] 1 test from 1 test case ran. (127 ms total)
[  PASSED  ] 1 test.
Tests that failed:
  WebKit2.WebKit2.CanHandleRequest
  WebKit2.WebKit2.DocumentStartUserScriptAlertCrashTest
  WebKit2.WebKit2.HitTestResultNodeHandle
  WebKit2.WebKit2.InjectedBundleBasic
""")

    def test_one_timeout(self):
        self.assertFailures(1, """Note: Google Test filter = WebViewDestructionWithHostWindow.DestroyViewWindowWithoutClose
[==========] Running 1 test from 1 test case.
[----------] Global test environment set-up.
[----------] 1 test from WebViewDestructionWithHostWindow
[ RUN      ] WebViewDestructionWithHostWindow.DestroyViewWindowWithoutClose
[       OK ] WebViewDestructionWithHostWindow.DestroyViewWindowWithoutClose (127 ms)
[----------] 1 test from WebViewDestructionWithHostWindow (127 ms total)

[----------] Global test environment tear-down
[==========] 1 test from 1 test case ran. (127 ms total)
[  PASSED  ] 1 test.
Tests that timed out:
  WebKit2.WebKit2.CanHandleRequest
""")

    def test_multiple_timeouts(self):
        self.assertFailures(4, """Note: Google Test filter = WebViewDestructionWithHostWindow.DestroyViewWindowWithoutClose
[==========] Running 1 test from 1 test case.
[----------] Global test environment set-up.
[----------] 1 test from WebViewDestructionWithHostWindow
[ RUN      ] WebViewDestructionWithHostWindow.DestroyViewWindowWithoutClose
[       OK ] WebViewDestructionWithHostWindow.DestroyViewWindowWithoutClose (127 ms)
[----------] 1 test from WebViewDestructionWithHostWindow (127 ms total)

[----------] Global test environment tear-down
[==========] 1 test from 1 test case ran. (127 ms total)
[  PASSED  ] 1 test.
Tests that timed out:
  WebKit2.WebKit2.CanHandleRequest
  WebKit2.WebKit2.DocumentStartUserScriptAlertCrashTest
  WebKit2.WebKit2.HitTestResultNodeHandle
  WebKit2.WebKit2.InjectedBundleBasic
""")

    def test_multiple_failures_and_timeouts(self):
        self.assertFailures(8, """Note: Google Test filter = WebViewDestructionWithHostWindow.DestroyViewWindowWithoutClose
[==========] Running 1 test from 1 test case.
[----------] Global test environment set-up.
[----------] 1 test from WebViewDestructionWithHostWindow
[ RUN      ] WebViewDestructionWithHostWindow.DestroyViewWindowWithoutClose
[       OK ] WebViewDestructionWithHostWindow.DestroyViewWindowWithoutClose (127 ms)
[----------] 1 test from WebViewDestructionWithHostWindow (127 ms total)

[----------] Global test environment tear-down
[==========] 1 test from 1 test case ran. (127 ms total)
[  PASSED  ] 1 test.
Tests that failed:
  WebKit2.WebKit2.CanHandleRequest
  WebKit2.WebKit2.DocumentStartUserScriptAlertCrashTest
  WebKit2.WebKit2.HitTestResultNodeHandle
Tests that timed out:
  WebKit2.WebKit2.InjectedBundleBasic
  WebKit2.WebKit2.LoadCanceledNoServerRedirectCallback
  WebKit2.WebKit2.MouseMoveAfterCrash
  WebKit2.WebKit2.ResponsivenessTimerDoesntFireEarly
  WebKit2.WebKit2.WebArchive
""")



# FIXME: We should run this file as part of test-webkitpy.
# Unfortunately test-webkitpy currently requires that unittests
# be located in a directory with a valid module name.
# 'build.webkit.org-config' is not a valid module name (due to '.' and '-')
# so for now this is a stand-alone test harness.
if __name__ == '__main__':
    BuildBotConfigLoader().load_config('master.cfg')
    unittest.main()
