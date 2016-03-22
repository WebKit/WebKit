#! /usr/bin/python

import sys
import os
import StringIO
import unittest
import make_passwords_json
import json

# Show DepricationWarnings come from buildbot - it isn't default with Python 2.7 or newer.
# See https://bugs.webkit.org/show_bug.cgi?id=90161 for details.
import warnings
warnings.simplefilter('default')

class BuildBotConfigLoader(object):
    def _add_webkitpy_to_sys_path(self):
        # When files are passed to the python interpreter on the command line (e.g. python test.py) __file__ is a relative path.
        absolute_file_path = os.path.abspath(__file__)
        webkit_org_config_dir = os.path.dirname(absolute_file_path)
        build_slave_support_dir = os.path.dirname(webkit_org_config_dir)
        webkit_tools_dir = os.path.dirname(build_slave_support_dir)
        scripts_dir = os.path.join(webkit_tools_dir, 'Scripts')
        sys.path.append(scripts_dir)

    def _mock_open(self, filename):
        if filename == 'passwords.json':
            return StringIO.StringIO(json.dumps(make_passwords_json.create_mock_slave_passwords_dict()))
        return __builtins__.open(filename)

    def _add_dependant_modules_to_sys_modules(self):
        from webkitpy.thirdparty.autoinstalled import buildbot
        sys.modules['buildbot'] = buildbot

    def load_config(self, master_cfg_path):
        # Before we can use webkitpy.thirdparty, we need to fix our path to include webkitpy.
        # FIXME: If we're ever run by test-webkitpy we won't need this step.
        self._add_webkitpy_to_sys_path()
        # master.cfg expects the buildbot module to be in sys.path.
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
        run_webkit_tests = RunWebKitTests()  # pylint is confused by the way we import the module ... pylint: disable-msg=E0602
        log_text = """
12:44:24.295 77706 13981 total leaks found for a total of 197,936 bytes.
12:44:24.295 77706 1 unique leaks found.
"""
        expected_incorrect_lines = [
            '13981 total leaks found for a total of 197,936 bytes.',
            '1 unique leaks found.',
        ]
        run_webkit_tests._parseRunWebKitTestsOutput(log_text)
        self.assertEqual(run_webkit_tests.incorrectLayoutLines, expected_incorrect_lines)

    def test_nrwt_missing_results(self):
        run_webkit_tests = RunWebKitTests()  # pylint is confused by the way we import the module ... pylint: disable-msg=E0602
        log_text = """
Expected to fail, but passed: (2)
  animations/additive-transform-animations.html
  animations/cross-fade-webkit-mask-box-image.html

Unexpected flakiness: text-only failures (2)
  fast/events/touch/touch-inside-iframe.html [ Failure Pass ]
  http/tests/inspector-enabled/console-clear-arguments-on-frame-navigation.html [ Failure Pass ]

Unexpected flakiness: timeouts (1)
  svg/text/foreignObject-repaint.xml [ Timeout Pass ]

Regressions: Unexpected missing results (1)
  svg/custom/zero-path-square-cap-rendering2.svg [ Missing ]

Regressions: Unexpected text-only failures (1)
  svg/custom/zero-path-square-cap-rendering2.svg [ Failure ]
"""
        run_webkit_tests._parseRunWebKitTestsOutput(log_text)
        self.assertEqual(set(run_webkit_tests.incorrectLayoutLines),
            set(['2 new passes', '3 flakes', '1 missing results', '1 failures']))

class StubStdio(object):
    def __init__(self, stdio):
        self._stdio = stdio

    def getText(self):
        return self._stdio


class StubRemoteCommand(object):
    def __init__(self, rc, stdio):
        self.rc = rc
        self.logs = {'stdio': StubStdio(stdio)}


class RunJavaScriptCoreTestsTest(unittest.TestCase):
    def assertResults(self, expected_result, expected_text, rc, stdio):
        cmd = StubRemoteCommand(rc, stdio)
        step = RunJavaScriptCoreTests()
        step.commandComplete(cmd)
        actual_results = step.evaluateCommand(cmd)
        actual_text = step.getText2(cmd, actual_results)

        self.assertEqual(expected_result, actual_results)
        self.assertEqual(actual_text, expected_text)

    def test_no_regressions_old_output(self):
        self.assertResults(SUCCESS, ["jscore-test"], 0, """Results for Mozilla tests:
    0 regressions found.
    0 tests fixed.
    OK.""")

    def test_no_failure_new_output(self):
        self.assertResults(SUCCESS, ["jscore-test"], 0, """Results for JSC stress tests:
    0 failures found.
    OK.""")

    def test_mozilla_failure_old_output(self):
        self.assertResults(FAILURE, ["1 JSC test failed"], 1, """Results for Mozilla tests:
    1 regression found.
    0 tests fixed.""")

    def test_mozilla_failures_old_output(self):
        self.assertResults(FAILURE, ["2 JSC tests failed"], 1, """Results for Mozilla tests:
    2 regressions found.
    0 tests fixed.""")

    def test_jsc_stress_failure_new_output(self):
        self.assertResults(FAILURE, ["1 JSC test failed"], 1,  """Results for JSC stress tests:
    1 failure found.""")

    def test_jsc_stress_failures_new_output(self):
        self.assertResults(FAILURE, ["5 JSC tests failed"], 1,  """Results for JSC stress tests:
    5 failures found.""")


class RunLLINTCLoopTestsTest(unittest.TestCase):
    def assertResults(self, expected_result, expected_text, rc, stdio):
        cmd = StubRemoteCommand(rc, stdio)
        step = RunLLINTCLoopTests()
        step.commandComplete(cmd)
        actual_results = step.evaluateCommand(cmd)
        actual_text = step.getText2(cmd, actual_results)

        self.assertEqual(expected_result, actual_results)
        self.assertEqual(actual_text, expected_text)

    def test_failures(self):
        self.assertResults(FAILURE, ['5 regressions found.'], 1,  '    5 regressions found.')

    def test_failure(self):
        self.assertResults(FAILURE, ['1 regression found.'], 1,  '    1 regression found.')

    def test_no_failure(self):
        self.assertResults(SUCCESS, ['webkit-jsc-cloop-test'], 0,  '    0 regressions found.')


class Run32bitJSCTestsTest(unittest.TestCase):
    def assertResults(self, expected_result, expected_text, rc, stdio):
        cmd = StubRemoteCommand(rc, stdio)
        step = Run32bitJSCTests()
        step.commandComplete(cmd)
        actual_results = step.evaluateCommand(cmd)
        actual_text = step.getText2(cmd, actual_results)

        self.assertEqual(expected_result, actual_results)
        self.assertEqual(actual_text, expected_text)

    def test_failures(self):
        self.assertResults(FAILURE, ['5 regressions found.'], 1,  '    5 failures found.')

    def test_failure(self):
        self.assertResults(FAILURE, ['1 regression found.'], 1,  '    1 failure found.')

    def test_no_failure(self):
        self.assertResults(SUCCESS, ['webkit-32bit-jsc-test'], 0,  '    0 failures found.')


class RunUnitTestsTest(unittest.TestCase):
    def assertFailures(self, expected_failure_count, stdio):
        if expected_failure_count:
            rc = 1
            expected_results = FAILURE
            plural_suffix = "" if expected_failure_count == 1 else "s"
            expected_text = '%d unit test%s failed or timed out' % (expected_failure_count, plural_suffix)
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


class SVNMirrorTest(unittest.TestCase):
    def setUp(self):
        self.config = json.load(open('config.json'))

    def get_SVNMirrorFromConfig(self, builderName):
        SVNMirror = None
        for builder in self.config['builders']:
            if builder['name'] == builderName:
                SVNMirror = builder.pop('SVNMirror', 'http://svn.webkit.org/repository/webkit/')
        return SVNMirror

    def test_CheckOutSource(self):
        # SVN mirror feature isn't unittestable now with source.oldsource.SVN(==source.SVN) , only with source.svn.SVN(==SVN)
        # https://bugs.webkit.org/show_bug.cgi?id=85887
        if issubclass(CheckOutSource, source.SVN):
            return

        # Compare CheckOutSource.baseURL with SVNMirror (or with the default URL) in config.json for all builders
        for builder in c['builders']:
            for buildStepFactory, kwargs in builder['factory'].steps:
                if str(buildStepFactory).split('.')[-1] == 'CheckOutSource':
                        CheckOutSourceInstance = buildStepFactory(**kwargs)
                        self.assertEqual(CheckOutSourceInstance.baseURL, self.get_SVNMirrorFromConfig(builder['name']))


class BuildStepsConstructorTest(unittest.TestCase):
    # "Passing a BuildStep subclass to factory.addStep is deprecated. Please pass a BuildStep instance instead.  Support will be dropped in v0.8.7."
    # It checks if all builder's all buildsteps can be insantiated after migration.
    # https://bugs.webkit.org/show_bug.cgi?id=89001
    # http://buildbot.net/buildbot/docs/0.8.6p1/manual/customization.html#writing-buildstep-constructors

    @staticmethod
    def generateTests():
        for builderNumber, builder in enumerate(c['builders']):
            for stepNumber, step in enumerate(builder['factory'].steps):
                builderName = builder['name'].encode('ascii', 'ignore')
                setattr(BuildStepsConstructorTest, 'test_builder%02d_step%02d' % (builderNumber, stepNumber), BuildStepsConstructorTest.createTest(builderName, step))

    @staticmethod
    def createTest(builderName, step):
        def doTest(self):
            try:
                buildStepFactory, kwargs = step
                buildStepFactory(**kwargs)
            except TypeError as e:
                buildStepName = str(buildStepFactory).split('.')[-1]
                self.fail("Error during instantiation %s buildstep for %s builder: %s\n" % (buildStepName, builderName, e))
        return doTest


expected_build_steps = {
    'Apple Win 7 Debug (Tests)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile', 'download-built-product', 'extract-built-product', 'jscore-test', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'Apple Win 7 Release (Tests)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile', 'download-built-product', 'extract-built-product', 'jscore-test', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'Apple Win Debug (Build)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile', 'compile-webkit', 'archive-built-product', 'upload', 'trigger'],
    'Apple Win Release (Build)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile', 'compile-webkit', 'archive-built-product', 'upload', 'trigger'],

    'Apple Yosemite 32-bit JSC (BuildAndTest)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'webkit-32bit-jsc-test'],
    'Apple Yosemite (Leaks)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'Apple Yosemite Debug (Build)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'archive-built-product', 'upload', 'trigger'],
    'Apple Yosemite Debug JSC (Tests)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'jscore-test'],
    'Apple Yosemite Debug WK1 (Tests)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'Apple Yosemite Debug WK2 (Tests)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'Apple Yosemite LLINT CLoop (BuildAndTest)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'webkit-jsc-cloop-test'],
    'Apple Yosemite Release (32-bit Build)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit'],
    'Apple Yosemite Release (Build)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'archive-built-product', 'upload', 'trigger'],
    'Apple Yosemite Release WK2 (Perf)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'perf-test'],
    'Apple Yosemite Release JSC (Tests)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'jscore-test'],
    'Apple Yosemite Release WK1 (Tests)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'Apple Yosemite Release WK2 (Tests)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],

    'Apple El Capitan 32-bit JSC (BuildAndTest)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'webkit-32bit-jsc-test'],
    'Apple El Capitan (Leaks)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'Apple El Capitan CMake Debug (Build)' :['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit'],
    'Apple El Capitan Debug (Build)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'archive-built-product', 'upload', 'trigger'],
    'Apple El Capitan Debug JSC (Tests)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'jscore-test'],
    'Apple El Capitan Debug WK1 (Tests)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'Apple El Capitan Debug WK2 (Tests)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'Apple El Capitan LLINT CLoop (BuildAndTest)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'webkit-jsc-cloop-test'],
    'Apple El Capitan Release (32-bit Build)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit'],
    'Apple El Capitan Release (Build)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'archive-built-product', 'upload', 'trigger'],
    'Apple El Capitan Release WK2 (Perf)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'perf-test'],
    'Apple El Capitan Release JSC (Tests)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'jscore-test'],
    'Apple El Capitan Release WK1 (Tests)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'Apple El Capitan Release WK2 (Tests)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],

    'Apple iOS 9 Release (Build)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit'],
    'Apple iOS 9 Simulator Release (Build)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'archive-built-product', 'upload', 'trigger'],
    'Apple iOS 9 Simulator Release WK1 (Tests)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'Apple iOS 9 Simulator Release WK2 (Tests)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],

    'Apple iOS 9 Simulator Debug (Build)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'archive-built-product', 'upload', 'trigger'],
    'Apple iOS 9 Simulator Debug WK1 (Tests)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'Apple iOS 9 Simulator Debug WK2 (Tests)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],

    'EFL Linux 64-bit Release WK2' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'jhbuild', 'compile-webkit', 'jscore-test', 'layout-test', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'archive-test-results', 'upload', 'MasterShellCommand', 'API tests'],
    'EFL Linux 64-bit Release WK2 (Perf)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'jhbuild', 'compile-webkit', 'perf-test'],

    'JSCOnly Linux ARMv7 Thumb2 Release' : ['configure build', 'wait-for-svn-server', 'svn', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'jscore-test'],
    'JSCOnly Linux ARMv7 Traditional Release' : ['configure build', 'wait-for-svn-server', 'svn', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'jscore-test'],
    'JSCOnly Linux AArch64 Release' : ['configure build', 'wait-for-svn-server', 'svn', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'jscore-test'],

    'GTK Linux 32-bit Release' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'jhbuild', 'compile-webkit', 'jscore-test', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'API tests', 'WebKit GObject DOM bindings API break tests'],
    'GTK Linux 64-bit Debug (Build)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'jhbuild', 'compile-webkit', 'archive-built-product', 'upload', 'trigger'],
    'GTK Linux 64-bit Debug (Tests)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'jhbuild', 'download-built-product', 'extract-built-product', 'jscore-test', 'layout-test', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'archive-test-results', 'upload', 'MasterShellCommand', 'API tests', 'WebKit GObject DOM bindings API break tests'],
    'GTK Linux 64-bit Release (Build)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'jhbuild', 'compile-webkit', 'archive-built-product', 'upload', 'trigger'],
    'GTK Linux 64-bit Release (Perf)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'jhbuild', 'download-built-product', 'extract-built-product', 'perf-test', 'benchmark-test'],
    'GTK Linux 64-bit Release (Tests)' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'jhbuild', 'download-built-product', 'extract-built-product', 'jscore-test', 'layout-test', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'archive-test-results', 'upload', 'MasterShellCommand', 'API tests', 'WebKit GObject DOM bindings API break tests'],
    'GTK Linux ARM Release' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'jhbuild', 'compile-webkit', 'jscore-test', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'API tests', 'WebKit GObject DOM bindings API break tests'],

    'WinCairo 64-Bit Release' : ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit'],
}


class BuildStepsTest(unittest.TestCase):
    @staticmethod
    def generateTests():
        for builder in c['builders']:
            builderName = builder['name'].encode('ascii', 'ignore')
            setattr(BuildStepsTest, 'test_builder %s' % builderName, BuildStepsTest.createTest(builder))

    @staticmethod
    def createTest(builder):
        def doTest(self):
            buildSteps = []
            for step in builder['factory'].steps:
                buildSteps.append(step[0].name)
            self.assertTrue(builder['name'] in expected_build_steps, "Missing expected result for builder: %s\n Actual result is %s" % (builder['name'], buildSteps))
            self.assertListEqual(expected_build_steps[builder['name']], buildSteps)

        return doTest

    def test_unnecessary_expected_results(self):
        builders = set()
        for builder in c['builders']:
            builders.add(builder['name'].encode('ascii', 'ignore'))

        for builder in expected_build_steps:
            self.assertTrue(builder in builders, "Builder %s doesn't exist, but has unnecessary expected results" % builder)


class RunGtkWebKitGObjectDOMBindingsAPIBreakTestsTest(unittest.TestCase):
    def assertResults(self, expected_missing, expected_new, stdio):
        expected_text = ""
        expected_results = SUCCESS
        if expected_new:
            expected_results = WARNINGS
        if expected_missing:
            expected_results = FAILURE

        cmd = StubRemoteCommand(0, stdio)
        step = RunGtkWebKitGObjectDOMBindingsAPIBreakTests()
        step.commandComplete(cmd)

        actual_results = step.evaluateCommand(cmd)
        self.assertEqual(expected_results, actual_results)
        self.assertEqual(expected_missing, step.missingAPI)
        self.assertEqual(expected_new, step.newAPI)

    def test_missing_and_new_api(self):
        self.assertResults(expected_missing=True, expected_new=True, stdio="""Missing API (API break!) detected in GObject DOM bindings
    gboolean webkit_dom_html_input_element_get_webkitdirectory(WebKitDOMHTMLInputElement*)
    gchar* webkit_dom_text_track_cue_get_text(WebKitDOMTextTrackCue*)

New API detected in GObject DOM bindings
    void webkit_dom_html_input_element_set_capture(WebKitDOMHTMLInputElement*, gboolean)
    gboolean webkit_dom_html_input_element_get_capture(WebKitDOMHTMLInputElement*)
    void webkit_dom_text_track_add_cue(WebKitDOMTextTrack*, WebKitDOMTextTrackCue*, GError**)
    gchar* webkit_dom_document_get_origin(WebKitDOMDocument*)""")

    def test_missing_api(self):
        self.assertResults(expected_missing=True, expected_new=False, stdio="""Missing API (API break!) detected in GObject DOM bindings
    gboolean webkit_dom_html_input_element_get_webkitdirectory(WebKitDOMHTMLInputElement*)
    gchar* webkit_dom_text_track_cue_get_text(WebKitDOMTextTrackCue*)""")

    def test_new_api(self):
        self.assertResults(expected_missing=False, expected_new=True, stdio="""New API detected in GObject DOM bindings
    void webkit_dom_html_input_element_set_capture(WebKitDOMHTMLInputElement*, gboolean)
    gboolean webkit_dom_html_input_element_get_capture(WebKitDOMHTMLInputElement*)
    void webkit_dom_text_track_add_cue(WebKitDOMTextTrack*, WebKitDOMTextTrackCue*, GError**)
    gchar* webkit_dom_document_get_origin(WebKitDOMDocument*)""")

    def test_success(self):
        self.assertResults(expected_missing=False, expected_new=False, stdio="")


class RunAndUploadPerfTestsTest(unittest.TestCase):
    def assertResults(self, rc, expected_text):
        cmd = StubRemoteCommand(rc, expected_text)
        step = RunAndUploadPerfTests()
        step.commandComplete(cmd)
        actual_results = step.evaluateCommand(cmd)
        actual_text = str(step.getText2(cmd, actual_results)[0])
        self.assertEqual(expected_text, actual_text)

    def test_success(self):
        self.assertResults(0, "perf-test")

    def test_tests_failed(self):
        self.assertResults(5, "5 perf tests failed")

    def test_build_bad_build(self):
        self.assertResults(255, "build not up to date")

    def test_build_bad_source_json(self):
        self.assertResults(254, "slave config JSON error")

    def test_build_bad_marge(self):
        self.assertResults(253, "output JSON merge error")

    def test_build_bad_failed_uploading(self):
        self.assertResults(252, "upload error")

    def test_build_bad_preparation(self):
        self.assertResults(251, "system dependency error")

    def test_buildbot_timeout(self):
        self.assertResults(-1, "timeout")


class RunBenchmarkTest(unittest.TestCase):
    def assertResults(self, rc, expected_text):
        cmd = StubRemoteCommand(rc, expected_text)
        step = RunBenchmarkTests()
        step.commandComplete(cmd)
        actual_results = step.evaluateCommand(cmd)
        actual_text = str(step.getText2(cmd, actual_results)[0])
        self.assertEqual(expected_text, actual_text)

    def test_success(self):
        self.assertResults(0, "benchmark-test")

    def test_tests_failed(self):
        self.assertResults(7, "7 benchmark tests failed")


# FIXME: We should run this file as part of test-webkitpy.
# Unfortunately test-webkitpy currently requires that unittests
# be located in a directory with a valid module name.
# 'build.webkit.org-config' is not a valid module name (due to '.' and '-')
# so for now this is a stand-alone test harness.
if __name__ == '__main__':
    BuildBotConfigLoader().load_config('master.cfg')
    BuildStepsConstructorTest.generateTests()
    BuildStepsTest.generateTests()
    unittest.main()
