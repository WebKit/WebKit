#! /usr/bin/env python

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

    def _add_dependent_modules_to_sys_modules(self):
        self._add_webkitpy_to_sys_path()
        from webkitpy.thirdparty.autoinstalled import buildbot
        sys.modules['buildbot'] = buildbot


class RunWebKitTestsTest(unittest.TestCase):
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

    def test_jsc_stress_failures_with_binary_results_output(self):
        self.assertResults(FAILURE, ["8 JSC tests failed"], 1,  """Results for JSC stress tests:
    5 failures found.
Results for JSC test binaries:
    3 failures found.""")

    def test_jsc_stress_failures_with_binary_result_output(self):
        self.assertResults(FAILURE, ["6 JSC tests failed"], 1,  """Results for JSC stress tests:
    5 failures found.
Results for JSC test binaries:
    1 failure found.""")


class RunTest262TestsTest(unittest.TestCase):
    def assertResults(self, expected_result, expected_text, rc, stdio):
        cmd = StubRemoteCommand(rc, stdio)
        step = RunTest262Tests()
        step.commandComplete(cmd)
        actual_results = step.evaluateCommand(cmd)
        actual_text = step.getText2(cmd, actual_results)

        self.assertEqual(expected_result, actual_results)
        self.assertEqual(actual_text, expected_text)

    def test_no_regressions_output(self):
        self.assertResults(SUCCESS, ["test262-test"], 0, """
-------------------------Settings------------------------
Test262 Dir: JSTests/test262
JSC: WebKitBuild/Release/jsc
DYLD_FRAMEWORK_PATH: WebKitBuild/Release
Child Processes: 32
Config file: Tools/Scripts/test262/config.yaml
Expectations file: Tools/Scripts/test262/expectations.yaml
--------------------------------------------------------

NEW PASS: test/annexB/built-ins/Date/prototype/getYear/length.js (default)
NEW PASS test/language/expressions/class/fields-after-same-line-static-method-computed-symbol-names.js (default)

Run with --save to save a new expectations file
Saved all the results in Tools/Scripts/test262/results.yaml
Summarizing results...
See summarized results in Tools/Scripts/test262/results-summary.txt

56071 tests ran
0 expected tests failed
0 tests newly fail
2546 tests newly pass
1241 test files skipped
Done in 247 seconds!
""")

    def test_failure_output(self):
        self.assertResults(FAILURE, ["1 Test262 test failed"], 0, """
-------------------------Settings------------------------
Test262 Dir: JSTests/test262
JSC: WebKitBuild/Release/jsc
DYLD_FRAMEWORK_PATH: WebKitBuild/Release
Child Processes: 32
Config file: Tools/Scripts/test262/config.yaml
Expectations file: Tools/Scripts/test262/expectations.yaml
--------------------------------------------------------

! NEW FAIL: test/annexB/built-ins/Date/prototype/getYear/length.js (default)
NEW PASS test/language/expressions/class/fields-after-same-line-static-method-computed-symbol-names.js (default)

Run with --save to save a new expectations file
Saved all the results in Tools/Scripts/test262/results.yaml
Summarizing results...
See summarized results in Tools/Scripts/test262/results-summary.txt

56071 tests ran
0 expected tests failed
0 tests newly fail
2546 tests newly pass
1241 test files skipped
Done in 247 seconds!
""")

    def test_failures_output(self):
        self.assertResults(FAILURE, ["2 Test262 tests failed"], 0, """
-------------------------Settings------------------------
Test262 Dir: JSTests/test262
JSC: WebKitBuild/Release/jsc
DYLD_FRAMEWORK_PATH: WebKitBuild/Release
Child Processes: 32
Config file: Tools/Scripts/test262/config.yaml
Expectations file: Tools/Scripts/test262/expectations.yaml
--------------------------------------------------------

NEW PASS test/language/statements/class/fields-after-same-line-static-async-gen-computed-names.js (default)
! NEW FAIL: test/annexB/built-ins/Date/prototype/getYear/length.js (default)
! NEW FAIL: test/annexB/built-ins/Date/prototype/getYear/length.js (strict mode)
NEW PASS test/language/expressions/class/fields-after-same-line-static-method-computed-symbol-names.js (default)

Run with --save to save a new expectations file
Saved all the results in Tools/Scripts/test262/results.yaml
Summarizing results...
See summarized results in Tools/Scripts/test262/results-summary.txt

56071 tests ran
0 expected tests failed
0 tests newly fail
2546 tests newly pass
1241 test files skipped
Done in 247 seconds!
""")


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


class RunAPITestsTest(unittest.TestCase):
    def assertFailures(self, expected_failure_count, stdio):
        if expected_failure_count:
            rc = 1
            expected_results = FAILURE
            plural_suffix = "" if expected_failure_count == 1 else "s"
            expected_text = '%d api test%s failed or timed out' % (expected_failure_count, plural_suffix)
        else:
            rc = 0
            expected_results = SUCCESS
            expected_text = 'run-api-tests'

        cmd = StubRemoteCommand(rc, stdio)
        step = RunAPITests()
        step.commandComplete(cmd)
        actual_results = step.evaluateCommand(cmd)
        actual_failure_count = step.failedTestCount
        actual_text = step.getText(cmd, actual_results)[0]

        self.assertEqual(expected_results, actual_results)
        self.assertEqual(expected_failure_count, actual_failure_count)
        self.assertEqual(expected_text, actual_text)

    def test_no_failures_or_timeouts(self):
        self.assertFailures(0, """...
worker/0 TestWTF.WTF_Variant.OperatorAmpersand Passed
worker/0 TestWTF.WTF_Variant.Ref Passed
worker/0 TestWTF.WTF_Variant.RefPtr Passed
worker/0 TestWTF.WTF_Variant.RetainPtr Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingMakeVisitor Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingSwitchOn Passed
worker/0 exiting
Ran 1888 tests of 1888 with 1888 successful
------------------------------
All tests successfully passed!
""")

    def test_no_failures_or_timeouts_with_disabled(self):
        self.assertFailures(0, """...
worker/0 TestWTF.WTF_Variant.OperatorAmpersand Passed
worker/0 TestWTF.WTF_Variant.Ref Passed
worker/0 TestWTF.WTF_Variant.RefPtr Passed
worker/0 TestWTF.WTF_Variant.RetainPtr Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingMakeVisitor Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingSwitchOn Passed
worker/0 exiting
Ran 1881 tests of 1888 with 1881 successful
------------------------------
All tests successfully passed!
""")

    def test_one_failure(self):
        self.assertFailures(1, """...
worker/0 TestWTF.WTF_Variant.OperatorAmpersand Passed
worker/0 TestWTF.WTF_Variant.Ref Passed
worker/0 TestWTF.WTF_Variant.RefPtr Passed
worker/0 TestWTF.WTF_Variant.RetainPtr Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingMakeVisitor Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingSwitchOn Passed
worker/0 exiting
Ran 1888 tests of 1888 with 1887 successful
------------------------------
Test suite failed

Crashed

    TestWTF.WTF.StringConcatenate_Unsigned
        **FAIL** WTF.StringConcatenate_Unsigned

        C:\\cygwin\\home\\buildbot\\slave\\win-release\\build\\Tools\\TestWebKitAPI\\Tests\\WTF\\StringConcatenate.cpp:84
        Value of: makeString("hello ", static_cast<unsigned short>(42) , " world")
          Actual: hello 42 world
        Expected: "hello * world"
        Which is: 74B00C9C

Testing completed, Exit status: 3
""")

    def test_multiple_failures(self):
        self.assertFailures(2, """...
worker/0 TestWTF.WTF_Variant.OperatorAmpersand Passed
worker/0 TestWTF.WTF_Variant.Ref Passed
worker/0 TestWTF.WTF_Variant.RefPtr Passed
worker/0 TestWTF.WTF_Variant.RetainPtr Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingMakeVisitor Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingSwitchOn Passed
worker/0 exiting
Ran 1888 tests of 1888 with 1886 successful
------------------------------
Test suite failed

Crashed

    TestWTF.WTF.StringConcatenate_Unsigned
        **FAIL** WTF.StringConcatenate_Unsigned

        C:\\cygwin\\home\\buildbot\\slave\\win-release\\build\\Tools\\TestWebKitAPI\\Tests\\WTF\\StringConcatenate.cpp:84
        Value of: makeString("hello ", static_cast<unsigned short>(42) , " world")
          Actual: hello 42 world
        Expected: "hello * world"
        Which is: 74B00C9C

    TestWTF.WTF_Expected.Unexpected
        **FAIL** WTF_Expected.Unexpected

        C:\cygwin\home\buildbot\slave\win-release\build\Tools\TestWebKitAPI\Tests\WTF\Expected.cpp:96
        Value of: s1
          Actual: oops
        Expected: s0
        Which is: oops

Testing completed, Exit status: 3
""")

    def test_one_timeout(self):
        self.assertFailures(1, """...
worker/0 TestWTF.WTF_Variant.OperatorAmpersand Passed
worker/0 TestWTF.WTF_Variant.Ref Passed
worker/0 TestWTF.WTF_Variant.RefPtr Passed
worker/0 TestWTF.WTF_Variant.RetainPtr Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingMakeVisitor Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingSwitchOn Passed
worker/0 exiting
Ran 1888 tests of 1888 with 1887 successful
------------------------------
Test suite failed

Timeout

     TestWTF.WTF_PoisonedUniquePtrForTriviallyDestructibleArrays.Assignment

Testing completed, Exit status: 3
""")

    def test_multiple_timeouts(self):
        self.assertFailures(2, """...
worker/0 TestWTF.WTF_Variant.OperatorAmpersand Passed
worker/0 TestWTF.WTF_Variant.Ref Passed
worker/0 TestWTF.WTF_Variant.RefPtr Passed
worker/0 TestWTF.WTF_Variant.RetainPtr Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingMakeVisitor Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingSwitchOn Passed
worker/0 exiting
Ran 1888 tests of 1888 with 1886 successful
------------------------------
Test suite failed

Timeout

    TestWTF.WTF_PoisonedUniquePtrForTriviallyDestructibleArrays.Assignment
    TestWTF.WTF_Lock.ContendedShortSection

Testing completed, Exit status: 3
""")

    def test_multiple_failures_and_timeouts(self):
        self.assertFailures(4, """...
worker/0 TestWTF.WTF_Variant.OperatorAmpersand Passed
worker/0 TestWTF.WTF_Variant.Ref Passed
worker/0 TestWTF.WTF_Variant.RefPtr Passed
worker/0 TestWTF.WTF_Variant.RetainPtr Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingMakeVisitor Passed
worker/0 TestWTF.WTF_Variant.VisitorUsingSwitchOn Passed
worker/0 exiting
Ran 1888 tests of 1888 with 1884 successful
------------------------------
Test suite failed

Crashed

    TestWTF.WTF.StringConcatenate_Unsigned
        **FAIL** WTF.StringConcatenate_Unsigned

        C:\\cygwin\\home\\buildbot\\slave\\win-release\\build\\Tools\\TestWebKitAPI\\Tests\\WTF\\StringConcatenate.cpp:84
        Value of: makeString("hello ", static_cast<unsigned short>(42) , " world")
          Actual: hello 42 world
        Expected: "hello * world"
        Which is: 74B00C9C

    TestWTF.WTF_Expected.Unexpected
        **FAIL** WTF_Expected.Unexpected

        C:\cygwin\home\buildbot\slave\win-release\build\Tools\TestWebKitAPI\Tests\WTF\Expected.cpp:96
        Value of: s1
          Actual: oops
        Expected: s0
        Which is: oops

Timeout

    TestWTF.WTF_PoisonedUniquePtrForTriviallyDestructibleArrays.Assignment
    TestWTF.WTF_Lock.ContendedShortSection

Testing completed, Exit status: 3
""")


class SVNMirrorTest(unittest.TestCase):
    def setUp(self):
        self.config = json.load(open('config.json'))

    def get_SVNMirrorFromConfig(self, builderName):
        SVNMirror = None
        for builder in self.config['builders']:
            if builder['name'] == builderName:
                SVNMirror = builder.pop('SVNMirror', 'https://svn.webkit.org/repository/webkit/')
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
    'Apple Win 7 Release (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile', 'download-built-product', 'extract-built-product', 'jscore-test', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'builtins-generator-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'Apple Win 7 Release (Build)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile', 'compile-webkit', 'archive-built-product', 'upload', 'transfer-to-s3', 'trigger'],
    'Apple Win 10 Debug (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile', 'download-built-product', 'extract-built-product', 'jscore-test', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'builtins-generator-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'Apple Win 10 Release (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile', 'download-built-product', 'extract-built-product', 'jscore-test', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'builtins-generator-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'Apple Win 10 Debug (Build)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile', 'compile-webkit', 'archive-built-product', 'upload', 'transfer-to-s3', 'trigger'],
    'Apple Win 10 Release (Build)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile', 'compile-webkit', 'archive-built-product', 'upload', 'transfer-to-s3', 'trigger'],

    'Apple Mojave (Leaks)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'Apple Mojave Debug (Build)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'archive-built-product', 'upload', 'archive-built-product', 'upload', 'transfer-to-s3', 'trigger'],
    'Apple Mojave Debug WK1 (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'builtins-generator-tests', 'dashboard-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'Apple Mojave Debug WK2 (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'builtins-generator-tests', 'dashboard-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'Apple Mojave Release (Build)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'archive-built-product', 'upload', 'archive-built-product', 'upload', 'transfer-to-s3', 'trigger'],
    'Apple Mojave Release WK1 (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'builtins-generator-tests', 'dashboard-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'Apple Mojave Release WK2 (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'builtins-generator-tests', 'dashboard-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],

    'Apple High Sierra Debug (Build)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'archive-built-product', 'upload', 'archive-built-product', 'upload', 'transfer-to-s3', 'trigger'],
    'Apple High Sierra Debug JSC (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'jscore-test'],
    'Apple High Sierra Debug Test262 (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'test262-test'],
    'Apple High Sierra Debug WK1 (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'builtins-generator-tests', 'dashboard-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'Apple High Sierra Debug WK2 (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'builtins-generator-tests', 'dashboard-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'Apple High Sierra LLINT CLoop (BuildAndTest)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'webkit-jsc-cloop-test'],
    'Apple High Sierra Release (Build)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'archive-built-product', 'upload', 'archive-built-product', 'upload', 'transfer-to-s3', 'trigger'],
    'Apple High Sierra Release JSC (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'jscore-test'],
    'Apple High Sierra Release Test262 (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'test262-test'],
    'Apple High Sierra Release WK1 (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'builtins-generator-tests', 'dashboard-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'Apple High Sierra Release WK2 (Perf)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'perf-test'],
    'Apple High Sierra Release WK2 (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'builtins-generator-tests', 'dashboard-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],

    'Apple iOS 12 Release (Build)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit'],
    'Apple iOS 12 Simulator Release (Build)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'archive-built-product', 'upload', 'archive-built-product', 'upload', 'transfer-to-s3', 'trigger'],
    'Apple iOS 12 Simulator Release WK2 (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'builtins-generator-tests', 'dashboard-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],

    'Apple iOS 12 Simulator Debug (Build)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'archive-built-product', 'upload', 'archive-built-product', 'upload', 'transfer-to-s3', 'trigger'],
    'Apple iOS 12 Simulator Debug WK2 (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'builtins-generator-tests', 'dashboard-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],

    'JSCOnly Linux ARMv7 Thumb2 Release': ['configure build', 'svn', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'jscore-test'],
    'JSCOnly Linux ARMv7 Thumb2 SoftFP Release': ['configure build', 'svn', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'jscore-test'],
    'JSCOnly Linux AArch64 Release': ['configure build', 'svn', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'jscore-test'],
    'JSCOnly Linux MIPS32el Release': ['configure build', 'svn', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'jscore-test'],

    'GTK Linux 64-bit Debug (Build)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'jhbuild', 'compile-webkit', 'archive-built-product', 'upload', 'transfer-to-s3', 'trigger'],
    'GTK Linux 64-bit Debug (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'jhbuild', 'download-built-product', 'extract-built-product', 'jscore-test', 'layout-test', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'builtins-generator-tests', 'dashboard-tests', 'archive-test-results', 'upload', 'MasterShellCommand', 'API tests', 'webdriver-test'],
    'GTK Linux 64-bit Release (Build)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'jhbuild', 'compile-webkit', 'archive-built-product', 'upload', 'generate-jsc-bundle', 'transfer-to-s3', 'trigger'],
    'GTK Linux 64-bit Release (Perf)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'jhbuild', 'download-built-product', 'extract-built-product', 'perf-test', 'benchmark-test'],
    'GTK Linux 64-bit Release (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'jhbuild', 'download-built-product', 'extract-built-product', 'jscore-test', 'layout-test', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'builtins-generator-tests', 'dashboard-tests', 'archive-test-results', 'upload', 'MasterShellCommand', 'API tests', 'webdriver-test'],
    'GTK Linux 64-bit Release Wayland (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'jhbuild', 'download-built-product', 'extract-built-product', 'jscore-test', 'layout-test', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'builtins-generator-tests', 'dashboard-tests', 'archive-test-results', 'upload', 'MasterShellCommand', 'API tests', 'webdriver-test'],
    'GTK Linux 64-bit Release Ubuntu LTS (Build)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit'],
    'GTK Linux 64-bit Release Debian Stable (Build)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit'],

    'WinCairo 64-bit JSC Debug (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'jscore-test'],
    'WinCairo 64-bit JSC Release (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'jscore-test'],
    'WinCairo 64-bit WKL Debug (Build)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'archive-built-product', 'upload', 'transfer-to-s3', 'trigger'],
    'WinCairo 64-bit WKL Debug (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'wincairo-requirements', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'builtins-generator-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],
    'WinCairo 64-bit WKL Release (Build)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'compile-webkit', 'archive-built-product', 'upload', 'transfer-to-s3', 'trigger'],
    'WinCairo 64-bit WKL Release (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'download-built-product', 'extract-built-product', 'wincairo-requirements', 'layout-test', 'run-api-tests', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'builtins-generator-tests', 'archive-test-results', 'upload', 'MasterShellCommand'],

    'WPE Linux 64-bit Release (Build)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'jhbuild', 'compile-webkit', 'archive-built-product', 'upload', 'transfer-to-s3', 'trigger'],
    'WPE Linux 64-bit Release (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'jhbuild', 'download-built-product', 'extract-built-product', 'jscore-test', 'layout-test', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'builtins-generator-tests', 'dashboard-tests', 'archive-test-results', 'upload', 'MasterShellCommand', 'API tests'],
    'WPE Linux 64-bit Debug (Build)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'jhbuild', 'compile-webkit', 'archive-built-product', 'upload', 'transfer-to-s3', 'trigger'],
    'WPE Linux 64-bit Debug (Tests)': ['configure build', 'svn', 'kill old processes', 'delete WebKitBuild directory', 'delete stale build files', 'jhbuild', 'download-built-product', 'extract-built-product', 'jscore-test', 'layout-test', 'webkitpy-test', 'webkitperl-test', 'bindings-generation-tests', 'builtins-generator-tests', 'dashboard-tests', 'archive-test-results', 'upload', 'MasterShellCommand', 'API tests'],
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
    BuildBotConfigLoader()._add_dependent_modules_to_sys_modules()
    from loadConfig import *
    c = {}
    loadBuilderConfig(c, test_mode_is_enabled=True)
    BuildStepsConstructorTest.generateTests()
    BuildStepsTest.generateTests()
    unittest.main()
