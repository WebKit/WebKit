# Copyright (C) 2017 Apple Inc. All rights reserved.
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

from buildbot.process import buildstep, factory, properties
from buildbot.steps import master, shell, source, transfer, trigger
from buildbot.status.builder import SUCCESS, FAILURE, WARNINGS, SKIPPED, EXCEPTION

from twisted.internet import defer

import os
import re
import json
import cStringIO
import urllib

APPLE_WEBKIT_AWS_PROXY = "http://proxy01.webkit.org:3128"
S3URL = "https://s3-us-west-2.amazonaws.com/"
WithProperties = properties.WithProperties


class TestWithFailureCount(shell.Test):
    failedTestsFormatString = "%d test%s failed"

    def countFailures(self, cmd):
        return 0

    def commandComplete(self, cmd):
        shell.Test.commandComplete(self, cmd)
        self.failedTestCount = self.countFailures(cmd)
        self.failedTestPluralSuffix = "" if self.failedTestCount == 1 else "s"

    def evaluateCommand(self, cmd):
        if self.failedTestCount:
            return FAILURE

        if cmd.rc != 0:
            return FAILURE

        return SUCCESS

    def getText(self, cmd, results):
        return self.getText2(cmd, results)

    def getText2(self, cmd, results):
        if results != SUCCESS and self.failedTestCount:
            return [self.failedTestsFormatString % (self.failedTestCount, self.failedTestPluralSuffix)]

        return [self.name]


class ConfigureBuild(buildstep.BuildStep):
    name = "configure build"
    description = ["configuring build"]
    descriptionDone = ["configured build"]

    def __init__(self, platform, configuration, architecture, buildOnly, additionalArguments, SVNMirror, *args, **kwargs):
        buildstep.BuildStep.__init__(self, *args, **kwargs)
        self.platform = platform
        if platform != 'jsc-only':
            self.platform = platform.split('-', 1)[0]
        self.fullPlatform = platform
        self.configuration = configuration
        self.architecture = architecture
        self.buildOnly = buildOnly
        self.additionalArguments = additionalArguments
        self.SVNMirror = SVNMirror
        self.addFactoryArguments(platform=platform, configuration=configuration, architecture=architecture, buildOnly=buildOnly, additionalArguments=additionalArguments, SVNMirror=SVNMirror)

    def start(self):
        self.setProperty("platform", self.platform)
        self.setProperty("fullPlatform", self.fullPlatform)
        self.setProperty("configuration", self.configuration)
        self.setProperty("architecture", self.architecture)
        self.setProperty("buildOnly", self.buildOnly)
        self.setProperty("additionalArguments", self.additionalArguments)
        self.setProperty("SVNMirror", self.SVNMirror)
        self.finished(SUCCESS)
        return defer.succeed(None)


class CheckOutSource(source.SVN):
    mode = "update"

    def __init__(self, SVNMirror, **kwargs):
        kwargs['baseURL'] = SVNMirror or "https://svn.webkit.org/repository/webkit/"
        kwargs['defaultBranch'] = "trunk"
        kwargs['mode'] = self.mode
        source.SVN.__init__(self, **kwargs)
        self.addFactoryArguments(SVNMirror=SVNMirror)


class WaitForSVNServer(shell.ShellCommand):
    name = "wait-for-svn-server"
    command = ["python", "./Tools/BuildSlaveSupport/wait-for-SVN-server.py", "-r", WithProperties("%(revision)s"), "-s", WithProperties("%(SVNMirror)s")]
    description = ["waiting for SVN server"]
    descriptionDone = ["SVN server is ready"]
    warnOnFailure = True

    def evaluateCommand(self, cmd):
        if cmd.rc != 0:
            return WARNINGS
        return SUCCESS


class InstallWin32Dependencies(shell.Compile):
    description = ["installing dependencies"]
    descriptionDone = ["installed dependencies"]
    command = ["perl", "./Tools/Scripts/update-webkit-auxiliary-libs"]


class KillOldProcesses(shell.Compile):
    name = "kill old processes"
    description = ["killing old processes"]
    descriptionDone = ["killed old processes"]
    command = ["python", "./Tools/BuildSlaveSupport/kill-old-processes", "buildbot"]


class CleanBuildIfScheduled(shell.Compile):
    name = "delete WebKitBuild directory"
    description = ["deleting WebKitBuild directory"]
    descriptionDone = ["deleted WebKitBuild directory"]
    command = ["python", "./Tools/BuildSlaveSupport/clean-build", WithProperties("--platform=%(fullPlatform)s"), WithProperties("--%(configuration)s")]

    def start(self):
        if not self.getProperty('is_clean'):
            self.hideStepIf = True
            return SKIPPED
        return shell.Compile.start(self)


class DeleteStaleBuildFiles(shell.Compile):
    name = "delete stale build files"
    description = ["deleting stale build files"]
    descriptionDone = ["deleted stale build files"]
    command = ["python", "./Tools/BuildSlaveSupport/delete-stale-build-files", WithProperties("--platform=%(fullPlatform)s"), WithProperties("--%(configuration)s")]

    def start(self):
        if self.getProperty('is_clean'):  # Nothing to be done if WebKitBuild had been removed.
            self.hideStepIf = True
            return SKIPPED
        return shell.Compile.start(self)


class InstallWinCairoDependencies(shell.ShellCommand):
    name = 'wincairo-requirements'
    description = ['updating wincairo dependencies']
    descriptionDone = ['updated wincairo dependencies']
    command = ['python', './Tools/Scripts/update-webkit-wincairo-libs.py']
    haltOnFailure = True


class InstallGtkDependencies(shell.ShellCommand):
    name = "jhbuild"
    description = ["updating gtk dependencies"]
    descriptionDone = ["updated gtk dependencies"]
    command = ["perl", "./Tools/Scripts/update-webkitgtk-libs"]
    haltOnFailure = True


class InstallWpeDependencies(shell.ShellCommand):
    name = "jhbuild"
    description = ["updating wpe dependencies"]
    descriptionDone = ["updated wpe dependencies"]
    command = ["perl", "./Tools/Scripts/update-webkitwpe-libs"]
    haltOnFailure = True


def appendCustomBuildFlags(step, platform, fullPlatform):
    if platform not in ('gtk', 'wincairo', 'ios', 'jsc-only', 'wpe'):
        return
    if fullPlatform.startswith('ios-simulator'):
        platform = 'ios-simulator'
    elif platform == 'ios':
        platform = 'device'
    step.setCommand(step.command + ['--' + platform])


class CompileWebKit(shell.Compile):
    command = ["perl", "./Tools/Scripts/build-webkit", WithProperties("--%(configuration)s")]
    env = {'MFLAGS': ''}
    name = "compile-webkit"
    description = ["compiling"]
    descriptionDone = ["compiled"]
    warningPattern = ".*arning: .*"

    def start(self):
        platform = self.getProperty('platform')
        buildOnly = self.getProperty('buildOnly')
        architecture = self.getProperty('architecture')
        additionalArguments = self.getProperty('additionalArguments')

        if additionalArguments:
            self.setCommand(self.command + additionalArguments)
        if platform in ('mac', 'ios') and architecture:
            self.setCommand(self.command + ['ARCHS=' + architecture])
            if platform == 'ios':
                self.setCommand(self.command + ['ONLY_ACTIVE_ARCH=NO'])
        if platform in ('mac', 'ios') and buildOnly:
            # For build-only bots, the expectation is that tests will be run on separate machines,
            # so we need to package debug info as dSYMs. Only generating line tables makes
            # this much faster than full debug info, and crash logs still have line numbers.
            self.setCommand(self.command + ['DEBUG_INFORMATION_FORMAT=dwarf-with-dsym'])
            self.setCommand(self.command + ['CLANG_DEBUG_INFORMATION_LEVEL=line-tables-only'])

        appendCustomBuildFlags(self, platform, self.getProperty('fullPlatform'))

        return shell.Compile.start(self)

    def createSummary(self, log):
        platform = self.getProperty('platform')
        if platform.startswith('mac'):
            warnings = []
            errors = []
            sio = cStringIO.StringIO(log.getText())
            for line in sio.readlines():
                if "arning:" in line:
                    warnings.append(line)
                if "rror:" in line:
                    errors.append(line)
            if warnings:
                self.addCompleteLog('warnings', "".join(warnings))
            if errors:
                self.addCompleteLog('errors', "".join(errors))


class CompileLLINTCLoop(CompileWebKit):
    command = ["perl", "./Tools/Scripts/build-jsc", "--cloop", WithProperties("--%(configuration)s")]


class Compile32bitJSC(CompileWebKit):
    command = ["perl", "./Tools/Scripts/build-jsc", "--32-bit", WithProperties("--%(configuration)s")]


class CompileJSCOnly(CompileWebKit):
    command = ["perl", "./Tools/Scripts/build-jsc", WithProperties("--%(configuration)s")]


class ArchiveBuiltProduct(shell.ShellCommand):
    command = ["python", "./Tools/BuildSlaveSupport/built-product-archive",
               WithProperties("--platform=%(fullPlatform)s"), WithProperties("--%(configuration)s"), "archive"]
    name = "archive-built-product"
    description = ["archiving built product"]
    descriptionDone = ["archived built product"]
    haltOnFailure = True


class ArchiveMinifiedBuiltProduct(ArchiveBuiltProduct):
    command = ["python", "./Tools/BuildSlaveSupport/built-product-archive",
               WithProperties("--platform=%(fullPlatform)s"), WithProperties("--%(configuration)s"), "archive", "--minify"]


class GenerateJSCBundle(shell.ShellCommand):
    command = ["python", "./Tools/Scripts/generate-jsc-bundle", "--builder-name", WithProperties("%(buildername)s"),
               WithProperties("--platform=%(fullPlatform)s"), WithProperties("--%(configuration)s"),
               WithProperties("--revision=%(got_revision)s"), "--remote-config-file", "../../remote-jsc-bundle-upload-config.json"]
    name = "generate-jsc-bundle"
    description = ["generating jsc bundle"]
    descriptionDone = ["generated jsc bundle"]
    haltOnFailure = False

class ExtractBuiltProduct(shell.ShellCommand):
    command = ["python", "./Tools/BuildSlaveSupport/built-product-archive",
               WithProperties("--platform=%(fullPlatform)s"), WithProperties("--%(configuration)s"), "extract"]
    name = "extract-built-product"
    description = ["extracting built product"]
    descriptionDone = ["extracted built product"]
    haltOnFailure = True


class UploadBuiltProduct(transfer.FileUpload):
    slavesrc = WithProperties("WebKitBuild/%(configuration)s.zip")
    masterdest = WithProperties("archives/%(fullPlatform)s-%(architecture)s-%(configuration)s/%(got_revision)s.zip")
    haltOnFailure = True

    def __init__(self, **kwargs):
        kwargs['slavesrc'] = self.slavesrc
        kwargs['masterdest'] = self.masterdest
        kwargs['mode'] = 0644
        kwargs['blocksize'] = 1024 * 256
        transfer.FileUpload.__init__(self, **kwargs)


class UploadMinifiedBuiltProduct(UploadBuiltProduct):
    slavesrc = WithProperties("WebKitBuild/minified-%(configuration)s.zip")
    masterdest = WithProperties("archives/%(fullPlatform)s-%(architecture)s-%(configuration)s/minified-%(got_revision)s.zip")


class DownloadBuiltProduct(shell.ShellCommand):
    command = ["python", "./Tools/BuildSlaveSupport/download-built-product",
        WithProperties("--platform=%(platform)s"), WithProperties("--%(configuration)s"),
        WithProperties(S3URL + "archives.webkit.org/%(fullPlatform)s-%(architecture)s-%(configuration)s/%(got_revision)s.zip")]
    name = "download-built-product"
    description = ["downloading built product"]
    descriptionDone = ["downloaded built product"]
    haltOnFailure = True
    flunkOnFailure = True

    def start(self):
        if 'apple' in self.getProperty('buildername').lower():
            self.slaveEnvironment['HTTPS_PROXY'] = APPLE_WEBKIT_AWS_PROXY  # curl env var to use a proxy
        return shell.ShellCommand.start(self)


class RunJavaScriptCoreTests(TestWithFailureCount):
    name = "jscore-test"
    description = ["jscore-tests running"]
    descriptionDone = ["jscore-tests"]
    jsonFileName = "jsc_results.json"
    command = ["perl", "./Tools/Scripts/run-javascriptcore-tests", "--no-build", "--no-fail-fast", "--json-output={0}".format(jsonFileName), WithProperties("--%(configuration)s")]
    failedTestsFormatString = "%d JSC test%s failed"
    logfiles = {"json": jsonFileName}

    def start(self):
        platform = self.getProperty('platform')
        # Linux bots have currently problems with JSC tests that try to use large amounts of memory.
        # Check: https://bugs.webkit.org/show_bug.cgi?id=175140
        if platform in ('gtk', 'wpe'):
            self.setCommand(self.command + ['--memory-limited'])
        # WinCairo uses the Windows command prompt, not Cygwin.
        elif platform == 'wincairo':
            self.setCommand(self.command + ['--test-writer=ruby'])

        appendCustomBuildFlags(self, platform, self.getProperty('fullPlatform'))
        return shell.Test.start(self)

    def countFailures(self, cmd):
        logText = cmd.logs['stdio'].getText()

        match = re.search(r'^Results for JSC stress tests:\r?\n\s+(\d+) failure', logText, re.MULTILINE)
        if match:
            return int(match.group(1))

        match = re.search(r'^Results for Mozilla tests:\r?\n\s+(\d+) regression', logText, re.MULTILINE)
        if match:
            return int(match.group(1))

        return 0


class RunRemoteJavaScriptCoreTests(RunJavaScriptCoreTests):
    def start(self):
        self.setCommand(self.command + ["--memory-limited", "--remote-config-file", "../../remote-jsc-tests-config.json"])
        return RunJavaScriptCoreTests.start(self)


class RunTest262Tests(TestWithFailureCount):
    name = "test262-test"
    description = ["test262-tests running"]
    descriptionDone = ["test262-tests"]
    failedTestsFormatString = "%d Test262 test%s failed"
    command = ["perl", "./Tools/Scripts/test262-runner", "--verbose", WithProperties("--%(configuration)s")]

    def start(self):
        appendCustomBuildFlags(self, self.getProperty('platform'), self.getProperty('fullPlatform'))
        return shell.Test.start(self)

    def countFailures(self, cmd):
        logText = cmd.logs['stdio'].getText()
        matches = re.findall(r'^\! NEW FAIL', logText, flags=re.MULTILINE)
        if matches:
            return len(matches)
        return 0


class RunWebKitTests(shell.Test):
    name = "layout-test"
    description = ["layout-tests running"]
    descriptionDone = ["layout-tests"]
    resultDirectory = "layout-test-results"
    command = ["python", "./Tools/Scripts/run-webkit-tests",
               "--no-build",
               "--no-show-results",
               "--no-new-test-results",
               "--clobber-old-results",
               "--builder-name", WithProperties("%(buildername)s"),
               "--build-number", WithProperties("%(buildnumber)s"),
               "--master-name", "webkit.org",
               "--test-results-server", "webkit-test-results.webkit.org",
               "--exit-after-n-crashes-or-timeouts", "50",
               "--exit-after-n-failures", "500",
               WithProperties("--%(configuration)s")]

    def start(self):
        platform = self.getProperty('platform')
        appendCustomBuildFlags(self, platform, self.getProperty('fullPlatform'))
        additionalArguments = self.getProperty('additionalArguments')

        self.setCommand(self.command + ["--results-directory", self.resultDirectory])
        self.setCommand(self.command + ['--debug-rwt-logging'])

        if platform == "win":
            self.setCommand(self.command + ['--batch-size', '100', '--root=' + os.path.join("WebKitBuild", self.getProperty('configuration'), "bin32")])

        if additionalArguments:
            self.setCommand(self.command + additionalArguments)
        return shell.Test.start(self)

    # FIXME: This will break if run-webkit-tests changes its default log formatter.
    nrwt_log_message_regexp = re.compile(r'\d{2}:\d{2}:\d{2}(\.\d+)?\s+\d+\s+(?P<message>.*)')

    def _strip_python_logging_prefix(self, line):
        match_object = self.nrwt_log_message_regexp.match(line)
        if match_object:
            return match_object.group('message')
        return line

    def _parseRunWebKitTestsOutput(self, logText):
        incorrectLayoutLines = []
        expressions = [
            ('flakes', re.compile(r'Unexpected flakiness.+\((\d+)\)')),
            ('new passes', re.compile(r'Expected to .+, but passed:\s+\((\d+)\)')),
            ('missing results', re.compile(r'Regressions: Unexpected missing results\s+\((\d+)\)')),
            ('failures', re.compile(r'Regressions: Unexpected.+\((\d+)\)')),
        ]
        testFailures = {}

        for line in logText.splitlines():
            if line.find('Exiting early') >= 0 or line.find('leaks found') >= 0:
                incorrectLayoutLines.append(self._strip_python_logging_prefix(line))
                continue
            for name, expression in expressions:
                match = expression.search(line)

                if match:
                    testFailures[name] = testFailures.get(name, 0) + int(match.group(1))
                    break

                # FIXME: Parse file names and put them in results

        for name in testFailures:
            incorrectLayoutLines.append(str(testFailures[name]) + ' ' + name)

        self.incorrectLayoutLines = incorrectLayoutLines

    def commandComplete(self, cmd):
        shell.Test.commandComplete(self, cmd)

        logText = cmd.logs['stdio'].getText()
        self._parseRunWebKitTestsOutput(logText)

    def evaluateCommand(self, cmd):
        result = SUCCESS

        if self.incorrectLayoutLines:
            if len(self.incorrectLayoutLines) == 1:
                line = self.incorrectLayoutLines[0]
                if line.find('were new') >= 0 or line.find('was new') >= 0 or line.find(' leak') >= 0:
                    return WARNINGS

            for line in self.incorrectLayoutLines:
                if line.find('flakes') >= 0 or line.find('new passes') >= 0 or line.find('missing results') >= 0:
                    result = WARNINGS
                else:
                    return FAILURE

        # Return code from Tools/Scripts/layout_tests/run_webkit_tests.py.
        # This means that an exception was raised when running run-webkit-tests and
        # was never handled.
        if cmd.rc == 254:
            return EXCEPTION
        if cmd.rc != 0:
            return FAILURE

        return result

    def getText(self, cmd, results):
        return self.getText2(cmd, results)

    def getText2(self, cmd, results):
        if results != SUCCESS and self.incorrectLayoutLines:
            return self.incorrectLayoutLines

        return [self.name]


class RunDashboardTests(RunWebKitTests):
    name = "dashboard-tests"
    description = ["dashboard-tests running"]
    descriptionDone = ["dashboard-tests"]
    resultDirectory = os.path.join(RunWebKitTests.resultDirectory, "dashboard-layout-test-results")

    def start(self):
        self.setCommand(self.command + ["--layout-tests-directory", "./Tools/BuildSlaveSupport/build.webkit.org-config/public_html/dashboard/Scripts/tests"])
        return RunWebKitTests.start(self)


class RunAPITests(TestWithFailureCount):
    name = "run-api-tests"
    description = ["api tests running"]
    descriptionDone = ["api-tests"]
    command = ["python", "./Tools/Scripts/run-api-tests", "--no-build", WithProperties("--%(configuration)s"), "--verbose"]
    failedTestsFormatString = "%d api test%s failed or timed out"

    def start(self):
        appendCustomBuildFlags(self, self.getProperty('platform'), self.getProperty('fullPlatform'))
        return shell.Test.start(self)

    def countFailures(self, cmd):
        log_text = cmd.logs['stdio'].getText()

        match = re.search(r'Ran (?P<ran>\d+) tests of (?P<total>\d+) with (?P<passed>\d+) successful', log_text)
        if not match:
            return -1
        return int(match.group('ran')) - int(match.group('passed'))


class RunPythonTests(TestWithFailureCount):
    name = "webkitpy-test"
    description = ["python-tests running"]
    descriptionDone = ["python-tests"]
    command = ["python", "./Tools/Scripts/test-webkitpy", "--verbose", WithProperties("--%(configuration)s")]
    failedTestsFormatString = "%d python test%s failed"

    def start(self):
        platform = self.getProperty('platform')
        # Python tests are flaky on the GTK builders, running them serially
        # helps and does not significantly prolong the cycle time.
        if platform == 'gtk':
            self.setCommand(self.command + ['--child-processes', '1'])
        # Python tests fail on windows bots when running more than one child process
        # https://bugs.webkit.org/show_bug.cgi?id=97465
        if platform == 'win':
            self.setCommand(self.command + ['--child-processes', '1'])
        return shell.Test.start(self)

    def countFailures(self, cmd):
        logText = cmd.logs['stdio'].getText()
        # We're looking for the line that looks like this: FAILED (failures=2, errors=1)
        regex = re.compile(r'^FAILED \((?P<counts>[^)]+)\)')
        for line in logText.splitlines():
            match = regex.match(line)
            if not match:
                continue
            return sum(int(component.split('=')[1]) for component in match.group('counts').split(', '))
        return 0


class RunPerlTests(TestWithFailureCount):
    name = "webkitperl-test"
    description = ["perl-tests running"]
    descriptionDone = ["perl-tests"]
    command = ["perl", "./Tools/Scripts/test-webkitperl"]
    failedTestsFormatString = "%d perl test%s failed"

    def countFailures(self, cmd):
        logText = cmd.logs['stdio'].getText()
        # We're looking for the line that looks like this: Failed 2/19 test programs. 5/363 subtests failed.
        regex = re.compile(r'^Failed \d+/\d+ test programs\. (?P<count>\d+)/\d+ subtests failed\.')
        for line in logText.splitlines():
            match = regex.match(line)
            if not match:
                continue
            return int(match.group('count'))
        return 0


class RunLLINTCLoopTests(TestWithFailureCount):
    name = "webkit-jsc-cloop-test"
    description = ["cloop-tests running"]
    descriptionDone = ["cloop-tests"]
    jsonFileName = "jsc_cloop.json"
    command = ["perl", "./Tools/Scripts/run-javascriptcore-tests", "--cloop", "--no-build", "--no-jsc-stress", "--no-fail-fast", "--json-output={0}".format(jsonFileName), WithProperties("--%(configuration)s")]
    failedTestsFormatString = "%d regression%s found."
    logfiles = {"json": jsonFileName}

    def countFailures(self, cmd):
        logText = cmd.logs['stdio'].getText()
        # We're looking for the line that looks like this: 0 regressions found.
        regex = re.compile(r'\s*(?P<count>\d+) regressions? found.')
        for line in logText.splitlines():
            match = regex.match(line)
            if not match:
                continue
            return int(match.group('count'))
        return 0


class Run32bitJSCTests(TestWithFailureCount):
    name = "webkit-32bit-jsc-test"
    description = ["32bit-jsc-tests running"]
    descriptionDone = ["32bit-jsc-tests"]
    jsonFileName = "jsc_32bit.json"
    command = ["perl", "./Tools/Scripts/run-javascriptcore-tests", "--32-bit", "--no-build", "--no-fail-fast", "--no-jit", "--no-testair", "--no-testb3", "--no-testmasm", "--json-output={0}".format(jsonFileName), WithProperties("--%(configuration)s")]
    failedTestsFormatString = "%d regression%s found."
    logfiles = {"json": jsonFileName}

    def countFailures(self, cmd):
        logText = cmd.logs['stdio'].getText()
        # We're looking for the line that looks like this: 0 failures found.
        regex = re.compile(r'\s*(?P<count>\d+) failures? found.')
        for line in logText.splitlines():
            match = regex.match(line)
            if not match:
                continue
            return int(match.group('count'))
        return 0


class RunBindingsTests(shell.Test):
    name = "bindings-generation-tests"
    description = ["bindings-tests running"]
    descriptionDone = ["bindings-tests"]
    command = ["python", "./Tools/Scripts/run-bindings-tests"]


class RunBuiltinsTests(shell.Test):
    name = "builtins-generator-tests"
    description = ["builtins-generator-tests running"]
    descriptionDone = ["builtins-generator-tests"]
    command = ["python", "./Tools/Scripts/run-builtins-generator-tests"]


class RunGLibAPITests(shell.Test):
    name = "API tests"
    description = ["API tests running"]
    descriptionDone = ["API tests"]

    def start(self):
        additionalArguments = self.getProperty("additionalArguments")
        if additionalArguments:
            self.command += additionalArguments
        self.setCommand(self.command)
        return shell.Test.start(self)

    def commandComplete(self, cmd):
        shell.Test.commandComplete(self, cmd)

        logText = cmd.logs['stdio'].getText()

        failedTests = 0
        crashedTests = 0
        timedOutTests = 0
        messages = []
        self.statusLine = []

        foundItems = re.findall("Unexpected failures \((\d+)\)", logText)
        if foundItems:
            failedTests = int(foundItems[0])
            messages.append("%d failures" % failedTests)

        foundItems = re.findall("Unexpected crashes \((\d+)\)", logText)
        if foundItems:
            crashedTests = int(foundItems[0])
            messages.append("%d crashes" % crashedTests)

        foundItems = re.findall("Unexpected timeouts \((\d+)\)", logText)
        if foundItems:
            timedOutTests = int(foundItems[0])
            messages.append("%d timeouts" % timedOutTests)

        foundItems = re.findall("Unexpected passes \((\d+)\)", logText)
        if foundItems:
            newPassTests = int(foundItems[0])
            messages.append("%d new passes" % newPassTests)

        self.totalFailedTests = failedTests + crashedTests + timedOutTests
        if messages:
            self.statusLine = ["API tests: %s" % ", ".join(messages)]

    def evaluateCommand(self, cmd):
        if self.totalFailedTests > 0:
            return FAILURE

        if cmd.rc != 0:
            return FAILURE

        return SUCCESS

    def getText(self, cmd, results):
        return self.getText2(cmd, results)

    def getText2(self, cmd, results):
        if results != SUCCESS and self.totalFailedTests > 0:
            return self.statusLine

        return [self.name]


class RunGtkAPITests(RunGLibAPITests):
    command = ["python", "./Tools/Scripts/run-gtk-tests", WithProperties("--%(configuration)s")]


class RunWPEAPITests(RunGLibAPITests):
    command = ["python", "./Tools/Scripts/run-wpe-tests", WithProperties("--%(configuration)s")]


class RunWebDriverTests(shell.Test):
    name = "webdriver-test"
    description = ["webdriver-tests running"]
    descriptionDone = ["webdriver-tests"]
    jsonFileName = "webdriver_tests.json"
    command = ["python", "./Tools/Scripts/run-webdriver-tests", "--json-output={0}".format(jsonFileName), WithProperties("--%(configuration)s")]
    logfiles = {"json": jsonFileName}

    def start(self):
        additionalArguments = self.getProperty('additionalArguments')
        if additionalArguments:
            self.setCommand(self.command + additionalArguments)

        appendCustomBuildFlags(self, self.getProperty('platform'), self.getProperty('fullPlatform'))
        return shell.Test.start(self)

    def commandComplete(self, cmd):
        shell.Test.commandComplete(self, cmd)
        logText = cmd.logs['stdio'].getText()

        self.failuresCount = 0
        self.newPassesCount = 0
        foundItems = re.findall("^Unexpected .+ \((\d+)\)", logText, re.MULTILINE)
        if foundItems:
            self.failuresCount = int(foundItems[0])
        foundItems = re.findall("^Expected to .+, but passed \((\d+)\)", logText, re.MULTILINE)
        if foundItems:
            self.newPassesCount = int(foundItems[0])

    def evaluateCommand(self, cmd):
        if self.failuresCount:
            return FAILURE

        if self.newPassesCount:
            return WARNINGS

        if cmd.rc != 0:
            return FAILURE

        return SUCCESS

    def getText(self, cmd, results):
        return self.getText2(cmd, results)

    def getText2(self, cmd, results):
        if results != SUCCESS and (self.failuresCount or self.newPassesCount):
            lines = []
            if self.failuresCount:
                lines.append("%d failures" % self.failuresCount)
            if self.newPassesCount:
                lines.append("%d new passes" % self.newPassesCount)
            return ["%s %s" % (self.name, ", ".join(lines))]

        return [self.name]


class RunWebKit1Tests(RunWebKitTests):
    def start(self):
        self.setCommand(self.command + ["--dump-render-tree"])

        return RunWebKitTests.start(self)


class RunWebKit1LeakTests(RunWebKit1Tests):
    want_stdout = False
    want_stderr = False
    warnOnWarnings = True

    def start(self):
        self.setCommand(self.command + ["--leaks"])
        return RunWebKit1Tests.start(self)


class RunAndUploadPerfTests(shell.Test):
    name = "perf-test"
    description = ["perf-tests running"]
    descriptionDone = ["perf-tests"]
    command = ["python", "./Tools/Scripts/run-perf-tests",
               "--output-json-path", "perf-test-results.json",
               "--slave-config-json-path", "../../perf-test-config.json",
               "--no-show-results",
               "--reset-results",
               "--test-results-server", "perf.webkit.org",
               "--builder-name", WithProperties("%(buildername)s"),
               "--build-number", WithProperties("%(buildnumber)s"),
               "--platform", WithProperties("%(fullPlatform)s"),
               "--no-build",
               WithProperties("--%(configuration)s")]

    def start(self):
        additionalArguments = self.getProperty("additionalArguments")
        if additionalArguments:
            self.command += additionalArguments
        self.setCommand(self.command)
        return shell.Test.start(self)

    def getText(self, cmd, results):
        return self.getText2(cmd, results)

    def getText2(self, cmd, results):
        if results != SUCCESS:
            if cmd.rc == -1 & 0xff:
                return ["build not up to date"]
            elif cmd.rc == -2 & 0xff:
                return ["slave config JSON error"]
            elif cmd.rc == -3 & 0xff:
                return ["output JSON merge error"]
            elif cmd.rc == -4 & 0xff:
                return ["upload error"]
            elif cmd.rc == -5 & 0xff:
                return ["system dependency error"]
            elif cmd.rc == -1:
                return ["timeout"]
            else:
                return ["%d perf tests failed" % cmd.rc]

        return [self.name]


class RunBenchmarkTests(shell.Test):
    name = "benchmark-test"
    description = ["benchmark tests running"]
    descriptionDone = ["benchmark tests"]
    command = ["python", "./Tools/Scripts/browserperfdash-benchmark", "--allplans",
               "--config-file", "../../browserperfdash-benchmark-config.txt",
               "--browser-version", WithProperties("r%(got_revision)s")]

    def start(self):
        platform = self.getProperty("platform")
        if platform == "gtk":
            self.command += ["--browser", "minibrowser-gtk"]
        self.setCommand(self.command)
        return shell.Test.start(self)

    def getText(self, cmd, results):
        return self.getText2(cmd, results)

    def getText2(self, cmd, results):
        if results != SUCCESS:
            return ["%d benchmark tests failed" % cmd.rc]
        return [self.name]


class ArchiveTestResults(shell.ShellCommand):
    command = ["python", "./Tools/BuildSlaveSupport/test-result-archive",
               WithProperties("--platform=%(platform)s"), WithProperties("--%(configuration)s"), "archive"]
    name = "archive-test-results"
    description = ["archiving test results"]
    descriptionDone = ["archived test results"]
    haltOnFailure = True


class UploadTestResults(transfer.FileUpload):
    slavesrc = "layout-test-results.zip"
    masterdest = WithProperties("public_html/results/%(buildername)s/r%(got_revision)s (%(buildnumber)s).zip")

    def __init__(self, **kwargs):
        kwargs['slavesrc'] = self.slavesrc
        kwargs['masterdest'] = self.masterdest
        kwargs['mode'] = 0644
        transfer.FileUpload.__init__(self, **kwargs)


class TransferToS3(master.MasterShellCommand):
    name = "transfer-to-s3"
    description = ["transferring to s3"]
    descriptionDone = ["transferred to s3"]
    archive = WithProperties("archives/%(fullPlatform)s-%(architecture)s-%(configuration)s/%(got_revision)s.zip")
    minifiedArchive = WithProperties("archives/%(fullPlatform)s-%(architecture)s-%(configuration)s/minified-%(got_revision)s.zip")
    identifier = WithProperties("%(fullPlatform)s-%(architecture)s-%(configuration)s")
    revision = WithProperties("%(got_revision)s")
    command = ["python", "./transfer-archive-to-s3", "--revision", revision, "--identifier", identifier, "--archive", archive]
    haltOnFailure = True

    def __init__(self, **kwargs):
        kwargs['command'] = self.command
        master.MasterShellCommand.__init__(self, **kwargs)

    def start(self):
        return master.MasterShellCommand.start(self)

    def finished(self, result):
        return master.MasterShellCommand.finished(self, result)


class ExtractTestResults(master.MasterShellCommand):
    zipFile = WithProperties("public_html/results/%(buildername)s/r%(got_revision)s (%(buildnumber)s).zip")
    resultDirectory = WithProperties("public_html/results/%(buildername)s/r%(got_revision)s (%(buildnumber)s)")
    descriptionDone = ["uploaded results"]

    def __init__(self, **kwargs):
        kwargs['command'] = ""
        master.MasterShellCommand.__init__(self, **kwargs)

    def resultDirectoryURL(self):
        return self.build.getProperties().render(self.resultDirectory).replace("public_html/", "/") + "/"

    def start(self):
        self.command = ["unzip", self.build.getProperties().render(self.zipFile), "-d", self.build.getProperties().render(self.resultDirectory)]
        return master.MasterShellCommand.start(self)

    def addCustomURLs(self):
        self.addURL("view layout test results", self.resultDirectoryURL() + "results.html")
        self.addURL("view dashboard test results", self.resultDirectoryURL() + "dashboard-layout-test-results/results.html")

    def finished(self, result):
        self.addCustomURLs()
        return master.MasterShellCommand.finished(self, result)


class ExtractTestResultsAndLeaks(ExtractTestResults):
    def addCustomURLs(self):
        ExtractTestResults.addCustomURLs(self)
        url = "/LeaksViewer/?url=" + urllib.quote(self.resultDirectoryURL(), safe="")
        self.addURL("view leaks", url)
