# Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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

from buildbot.plugins import steps, util
from buildbot.process import buildstep, logobserver, properties
from buildbot.process.results import Results, SUCCESS, FAILURE, CANCELLED, WARNINGS, SKIPPED, EXCEPTION, RETRY
from buildbot.steps import master, shell, transfer, trigger
from buildbot.steps.source import git
from twisted.internet import defer

import json
import os
import re
import socket
import sys

if sys.version_info < (3, 5):
    print('ERROR: Please use Python 3. This code is not compatible with Python 2.')
    sys.exit(1)

BUILD_WEBKIT_HOSTNAME = 'build.webkit.org'
COMMITS_INFO_URL = 'https://commits.webkit.org/'
CURRENT_HOSTNAME = socket.gethostname().strip()
RESULTS_WEBKIT_URL = 'https://results.webkit.org'
RESULTS_SERVER_API_KEY = 'RESULTS_SERVER_API_KEY'
S3URL = 'https://s3-us-west-2.amazonaws.com/'
WithProperties = properties.WithProperties
Interpolate = properties.Interpolate
THRESHOLD_FOR_EXCESSIVE_LOGS = 1000000
MSG_FOR_EXCESSIVE_LOGS = f'Stopped due to excessive logging, limit: {THRESHOLD_FOR_EXCESSIVE_LOGS}'


class CustomFlagsMixin(object):

    def appendCrossTargetFlags(self, additionalArguments):
        if additionalArguments:
            for additionalArgument in additionalArguments:
                if additionalArgument.startswith('--cross-target='):
                    self.command.append(additionalArgument)
                    return

    def appendCustomBuildFlags(self, platform, fullPlatform):
        if platform not in ('gtk', 'wincairo', 'ios', 'jsc-only', 'wpe', 'playstation', 'tvos', 'watchos',):
            return
        if 'simulator' in fullPlatform:
            platform = platform + '-simulator'
        elif platform in ['ios', 'tvos', 'watchos']:
            platform = platform + '-device'
        self.setCommand(self.command + ['--' + platform])

    def appendCustomTestingFlags(self, platform, device_model):
        if platform not in ('gtk', 'wincairo', 'ios', 'jsc-only', 'wpe'):
            return
        if device_model == 'iphone':
            device_model = 'iphone-simulator'
        elif device_model == 'ipad':
            device_model = 'ipad-simulator'
        else:
            device_model = platform
        self.setCommand(self.command + ['--' + device_model])


class ParseByLineLogObserver(logobserver.LineConsumerLogObserver):
    """A pretty wrapper for LineConsumerLogObserver to avoid
       repeatedly setting up generator processors."""
    def __init__(self, consumeLineFunc):
        if not callable(consumeLineFunc):
            raise Exception("Error: ParseByLineLogObserver requires consumeLineFunc to be callable.")
        self.consumeLineFunc = consumeLineFunc
        super(ParseByLineLogObserver, self).__init__(self.consumeLineGenerator)

    def consumeLineGenerator(self):
        """The generator LineConsumerLogObserver expects."""
        try:
            while True:
                stream, line = yield
                self.consumeLineFunc(line)
        except GeneratorExit:
            return


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

    def getResultSummary(self):
        status = self.name

        if self.results != SUCCESS:
            if self.failedTestCount:
                status = self.failedTestsFormatString % (self.failedTestCount, self.failedTestPluralSuffix)
            else:
                status += ' ({})'.format(Results[self.results])

        return {'step': status}


class ConfigureBuild(buildstep.BuildStep):
    name = "configure-build"
    description = ["configuring build"]
    descriptionDone = ["configured build"]

    def __init__(self, platform, configuration, architecture, buildOnly, additionalArguments, device_model, *args, **kwargs):
        buildstep.BuildStep.__init__(self, *args, **kwargs)
        self.platform = platform
        if platform != 'jsc-only':
            self.platform = platform.split('-', 1)[0]
        self.fullPlatform = platform
        self.configuration = configuration
        self.architecture = architecture
        self.buildOnly = buildOnly
        self.additionalArguments = additionalArguments
        self.device_model = device_model

    def start(self):
        self.setProperty("platform", self.platform)
        self.setProperty("fullPlatform", self.fullPlatform)
        self.setProperty("configuration", self.configuration)
        self.setProperty("architecture", self.architecture)
        self.setProperty("buildOnly", self.buildOnly)
        self.setProperty("additionalArguments", self.additionalArguments)
        self.setProperty("device_model", self.device_model)
        self.finished(SUCCESS)
        return defer.succeed(None)


class CheckOutSource(git.Git):
    name = 'clean-and-update-working-directory'
    CHECKOUT_DELAY_AND_MAX_RETRIES_PAIR = (0, 2)
    haltOnFailure = False

    def __init__(self, repourl='https://github.com/WebKit/WebKit.git', **kwargs):
        super(CheckOutSource, self).__init__(repourl=repourl,
                                             retry=self.CHECKOUT_DELAY_AND_MAX_RETRIES_PAIR,
                                             timeout=2 * 60 * 60,
                                             logEnviron=False,
                                             method='clean',
                                             progress=True,
                                             **kwargs)

    def getResultSummary(self):
        if self.results == FAILURE:
            self.build.addStepsAfterCurrentStep([CleanUpGitIndexLock()])

        if self.results != SUCCESS:
            return {'step': 'Failed to updated working directory'}
        else:
            return {'step': 'Cleaned and updated working directory'}

    @defer.inlineCallbacks
    def run(self):
        rc = yield super().run()
        if rc == SUCCESS:
            yield self._dovccmd(['remote', 'set-url', '--push', 'origin', 'PUSH_DISABLED_BY_ADMIN'])
        defer.returnValue(rc)


class CleanUpGitIndexLock(shell.ShellCommandNewStyle):
    name = 'clean-git-index-lock'
    command = ['rm', '-f', '.git/index.lock']
    descriptionDone = ['Deleted .git/index.lock']

    def __init__(self, **kwargs):
        super(CleanUpGitIndexLock, self).__init__(timeout=2 * 60, logEnviron=False, **kwargs)

    @defer.inlineCallbacks
    def run(self):
        platform = self.getProperty('platform', '*')
        if platform == 'wincairo':
            self.command = ['del', r'.git\index.lock']

        rc = yield super().run()
        if rc != SUCCESS:
            self.build.buildFinished(['Git issue, retrying build'], RETRY)
        defer.returnValue(rc)


class CheckOutSpecificRevision(shell.ShellCommandNewStyle):
    name = 'checkout-specific-revision'
    descriptionDone = ['Checked out required revision']
    flunkOnFailure = True
    haltOnFailure = True

    def __init__(self, **kwargs):
        super(CheckOutSpecificRevision, self).__init__(logEnviron=False, **kwargs)

    def doStepIf(self, step):
        return self.getProperty('user_provided_git_hash', False)

    def hideStepIf(self, results, step):
        return not self.doStepIf(step)

    def run(self):
        self.command = ['git', 'checkout', self.getProperty('user_provided_git_hash')]
        return super().run()


class InstallWin32Dependencies(shell.Compile):
    description = ["installing dependencies"]
    descriptionDone = ["installed dependencies"]
    command = ["perl", "Tools/Scripts/update-webkit-auxiliary-libs"]


class KillOldProcesses(shell.Compile):
    name = "kill-old-processes"
    description = ["killing old processes"]
    descriptionDone = ["killed old processes"]
    command = ["python3", "Tools/CISupport/kill-old-processes", "buildbot"]


class PruneCoreSymbolicationdCacheIfTooLarge(shell.ShellCommandNewStyle):
    name = "prune-coresymbolicationd-cache-if-too-large"
    description = ["pruning coresymbolicationd cache to < 10GB"]
    descriptionDone = ["pruned coresymbolicationd cache"]
    flunkOnFailure = False
    haltOnFailure = False
    command = ["python3", "Tools/Scripts/delete-if-too-large",
               "/System/Library/Caches/com.apple.coresymbolicationd"]


class TriggerCrashLogSubmission(shell.Compile):
    name = "trigger-crash-log-submission"
    description = ["triggering crash log submission"]
    descriptionDone = ["triggered crash log submission"]
    command = ["python3", "Tools/CISupport/trigger-crash-log-submission"]


class WaitForCrashCollection(shell.Compile):
    name = "wait-for-crash-collection"
    description = ["waiting for crash collection to quiesce"]
    descriptionDone = ["crash collection has quiesced"]
    command = ["python3", "Tools/CISupport/wait-for-crash-collection", "--timeout", str(5 * 60)]


class CleanBuildIfScheduled(shell.Compile):
    name = "delete-WebKitBuild-directory"
    description = ["deleting WebKitBuild directory"]
    descriptionDone = ["deleted WebKitBuild directory"]
    command = ["python3", "Tools/CISupport/clean-build", WithProperties("--platform=%(fullPlatform)s"), WithProperties("--%(configuration)s")]

    def start(self):
        if not self.getProperty('is_clean'):
            self.hideStepIf = True
            return SKIPPED
        return shell.Compile.start(self)


class DeleteStaleBuildFiles(shell.Compile):
    name = "delete-stale-build-files"
    description = ["deleting stale build files"]
    descriptionDone = ["deleted stale build files"]
    command = ["python3", "Tools/CISupport/delete-stale-build-files", WithProperties("--platform=%(fullPlatform)s"), WithProperties("--%(configuration)s")]

    def start(self):
        if self.getProperty('is_clean'):  # Nothing to be done if WebKitBuild had been removed.
            self.hideStepIf = True
            return SKIPPED
        return shell.Compile.start(self)


class InstallWinCairoDependencies(shell.ShellCommandNewStyle):
    name = 'wincairo-requirements'
    description = ['updating wincairo dependencies']
    descriptionDone = ['updated wincairo dependencies']
    command = ['python3', './Tools/Scripts/update-webkit-wincairo-libs.py']
    haltOnFailure = True


class InstallGtkDependencies(shell.ShellCommandNewStyle, CustomFlagsMixin):
    name = "jhbuild"
    description = ["updating gtk dependencies"]
    descriptionDone = ["updated gtk dependencies"]
    command = ["perl", "Tools/Scripts/update-webkitgtk-libs", WithProperties("--%(configuration)s")]
    haltOnFailure = True

    def run(self):
        self.appendCrossTargetFlags(self.getProperty('additionalArguments'))
        return super().run()


class InstallWpeDependencies(shell.ShellCommandNewStyle, CustomFlagsMixin):
    name = "jhbuild"
    description = ["updating wpe dependencies"]
    descriptionDone = ["updated wpe dependencies"]
    command = ["perl", "Tools/Scripts/update-webkitwpe-libs", WithProperties("--%(configuration)s")]
    haltOnFailure = True

    def run(self):
        self.appendCrossTargetFlags(self.getProperty('additionalArguments'))
        return super().run()


class CompileWebKit(shell.Compile, CustomFlagsMixin):
    command = ["perl", "Tools/Scripts/build-webkit", "--no-fatal-warnings", WithProperties("--%(configuration)s")]
    env = {'MFLAGS': ''}
    name = "compile-webkit"
    description = ["compiling"]
    descriptionDone = ["compiled"]
    warningPattern = ".*arning: .*"
    cancelled_due_to_huge_logs = False
    line_count = 0

    def start(self):
        platform = self.getProperty('platform')
        buildOnly = self.getProperty('buildOnly')
        architecture = self.getProperty('architecture')
        additionalArguments = self.getProperty('additionalArguments')

        self.log_observer = ParseByLineLogObserver(self.parseOutputLine)
        self.addLogObserver('stdio', self.log_observer)

        if additionalArguments:
            self.setCommand(self.command + additionalArguments)
        if platform in ('mac', 'ios', 'tvos', 'watchos'):
            # FIXME: Once WK_VALIDATE_DEPENDENCIES is set via xcconfigs, it can
            # be removed here. We can't have build-webkit pass this by default
            # without invalidating local builds made by Xcode, and we set it
            # via xcconfigs until all building of Xcode-based webkit is done in
            # workspaces (rdar://88135402).
            if architecture:
                self.setCommand(self.command + ['--architecture', architecture])
            self.setCommand(self.command + ['WK_VALIDATE_DEPENDENCIES=YES'])
            if buildOnly:
                # For build-only bots, the expectation is that tests will be run on separate machines,
                # so we need to package debug info as dSYMs. Only generating line tables makes
                # this much faster than full debug info, and crash logs still have line numbers.
                # Some projects (namely lldbWebKitTester) require full debug info, and may override this.
                self.setCommand(self.command + ['DEBUG_INFORMATION_FORMAT=dwarf-with-dsym'])
                self.setCommand(self.command + ['CLANG_DEBUG_INFORMATION_LEVEL=$(WK_OVERRIDE_DEBUG_INFORMATION_LEVEL:default=line-tables-only)'])
        if platform == 'gtk':
            prefix = os.path.join("/app", "webkit", "WebKitBuild", self.getProperty("configuration").title(), "install")
            self.setCommand(self.command + [f'--prefix={prefix}'])

        self.appendCustomBuildFlags(platform, self.getProperty('fullPlatform'))

        return shell.Compile.start(self)

    def buildCommandKwargs(self, warnings):
        kwargs = super(CompileWebKit, self).buildCommandKwargs(warnings)
        kwargs['timeout'] = 60 * 30
        return kwargs

    def parseOutputLine(self, line):
        self.line_count += 1
        if self.line_count == THRESHOLD_FOR_EXCESSIVE_LOGS:
            self.handleExcessiveLogging()
            return

        if "arning:" in line:
            self._addToLog('warnings', line + '\n')
        if "rror:" in line:
            self._addToLog('errors', line + '\n')

    def handleExcessiveLogging(self):
        build_url = f'{self.master.config.buildbotURL}#/builders/{self.build._builderid}/builds/{self.build.number}'
        print(f'\n{MSG_FOR_EXCESSIVE_LOGS}, {build_url}\n')
        self.cancelled_due_to_huge_logs = True
        self.build.stopBuild(reason=MSG_FOR_EXCESSIVE_LOGS, results=FAILURE)
        self.build.buildFinished([MSG_FOR_EXCESSIVE_LOGS], FAILURE)

    @defer.inlineCallbacks
    def _addToLog(self, logName, message):
        try:
            log = self.getLog(logName)
        except KeyError:
            log = yield self.addLog(logName)
        log.addStdout(message)

    def evaluateCommand(self, cmd):
        rc = super().evaluateCommand(cmd)
        if rc in (SUCCESS, WARNINGS) and self.getProperty('user_provided_git_hash'):
            self.build.addStepsAfterCurrentStep([ArchiveMinifiedBuiltProduct(), UploadMinifiedBuiltProduct(), TransferToS3(terminate_build=True)])
        return rc

    def getResultSummary(self):
        if self.cancelled_due_to_huge_logs:
            return {'step': MSG_FOR_EXCESSIVE_LOGS, 'build': MSG_FOR_EXCESSIVE_LOGS}
        if self.results == FAILURE:
            return {'step': f'Failed {self.name}'}
        return shell.Compile.getResultSummary(self)


class CompileLLINTCLoop(CompileWebKit):
    command = ["perl", "Tools/Scripts/build-jsc", "--cloop", WithProperties("--%(configuration)s")]


class Compile32bitJSC(CompileWebKit):
    name = 'compile-jsc-32bit'
    command = ["perl", "Tools/Scripts/build-jsc", "--32-bit", WithProperties("--%(configuration)s")]


class CompileJSCOnly(CompileWebKit):
    name = 'compile-jsc'
    command = ["perl", "Tools/Scripts/build-jsc", WithProperties("--%(configuration)s")]


class InstallBuiltProduct(shell.ShellCommandNewStyle):
    name = 'install-built-product'
    description = ['Installing Built Product']
    descriptionDone = ['Installed Built Product']
    command = ["python3", "Tools/Scripts/install-built-product",
               WithProperties("--platform=%(fullPlatform)s"), WithProperties("--%(configuration)s")]


class ArchiveBuiltProduct(shell.ShellCommandNewStyle, CustomFlagsMixin):
    command = ["python3", "Tools/CISupport/built-product-archive",
               WithProperties("--platform=%(fullPlatform)s"), WithProperties("--%(configuration)s")]
    name = "archive-built-product"
    description = ["archiving built product"]
    descriptionDone = ["archived built product"]
    haltOnFailure = True

    def run(self):
        self.appendCrossTargetFlags(self.getProperty('additionalArguments'))
        self.command.append("archive")
        return super().run()


class ArchiveMinifiedBuiltProduct(ArchiveBuiltProduct):
    name = 'archive-minified-built-product'
    command = ["python3", "Tools/CISupport/built-product-archive",
               WithProperties("--platform=%(fullPlatform)s"), WithProperties("--%(configuration)s"), "--minify"]


# UploadBuiltProductViaSftp() is still unused. Check HOWTO_config_SFTP_uploads.md about how to enable it.
class UploadBuiltProductViaSftp(shell.ShellCommandNewStyle):
    command = ["python3", "Tools/CISupport/Shared/transfer-archive-via-sftp",
               "--remote-config-file", "../../remote-built-product-upload-config.json",
               "--user-name", WithProperties("%(buildername)s"),
               "--remote-dir", WithProperties("%(buildername)s"),
               "--remote-file", WithProperties("%(archive_revision)s.zip"),
               WithProperties("WebKitBuild/%(configuration)s.zip")]
    name = "upload-built-product-via-sftp"
    description = ["uploading built product via sftp"]
    descriptionDone = ["uploaded built product via sftp"]
    haltOnFailure = True


class UploadMiniBrowserBundleViaSftp(shell.ShellCommandNewStyle):
    command = ["python3", "Tools/CISupport/Shared/transfer-archive-via-sftp",
               "--remote-config-file", "../../remote-minibrowser-bundle-upload-config.json",
               "--remote-file", WithProperties("MiniBrowser_%(fullPlatform)s_%(archive_revision)s.zip"),
               WithProperties("WebKitBuild/MiniBrowser_%(fullPlatform)s_%(configuration)s.zip")]
    name = "upload-minibrowser-bundle-via-sftp"
    description = ["uploading minibrowser bundle via sftp"]
    descriptionDone = ["uploaded minibrowser bundle via sftp"]
    haltOnFailure = False


class UploadJSCBundleViaSftp(shell.ShellCommandNewStyle):
    command = ["python3", "Tools/CISupport/Shared/transfer-archive-via-sftp",
               "--remote-config-file", "../../remote-jsc-bundle-upload-config.json",
               "--remote-file", WithProperties("%(archive_revision)s.zip"),
               WithProperties("WebKitBuild/jsc_%(fullPlatform)s_%(configuration)s.zip")]
    name = "upload-jsc-bundle-via-sftp"
    description = ["uploading jsc bundle via sftp"]
    descriptionDone = ["uploaded jsc bundle via sftp"]
    haltOnFailure = False


class GenerateJSCBundle(shell.ShellCommandNewStyle):
    command = ["Tools/Scripts/generate-bundle", "--builder-name", WithProperties("%(buildername)s"),
               "--bundle=jsc", "--syslibs=bundle-all", WithProperties("--platform=%(fullPlatform)s"),
               WithProperties("--%(configuration)s"), WithProperties("--revision=%(archive_revision)s")]
    name = "generate-jsc-bundle"
    description = ["generating jsc bundle"]
    descriptionDone = ["generated jsc bundle"]
    haltOnFailure = False

    @defer.inlineCallbacks
    def run(self):
        rc = yield super().run()
        if rc in (SUCCESS, WARNINGS):
            self.build.addStepsAfterCurrentStep([UploadJSCBundleViaSftp()])
        defer.returnValue(rc)


class GenerateMiniBrowserBundle(shell.ShellCommandNewStyle):
    command = ["Tools/Scripts/generate-bundle", "--builder-name", WithProperties("%(buildername)s"),
               "--bundle=MiniBrowser", WithProperties("--platform=%(fullPlatform)s"),
               WithProperties("--%(configuration)s"), WithProperties("--revision=%(archive_revision)s")]
    name = "generate-minibrowser-bundle"
    description = ["generating minibrowser bundle"]
    descriptionDone = ["generated minibrowser bundle"]
    haltOnFailure = False

    @defer.inlineCallbacks
    def run(self):
        rc = yield super().run()
        if rc in (SUCCESS, WARNINGS):
            self.build.addStepsAfterCurrentStep([UploadMiniBrowserBundleViaSftp()])
        defer.returnValue(rc)


class ExtractBuiltProduct(shell.ShellCommandNewStyle):
    command = ["python3", "Tools/CISupport/built-product-archive",
               WithProperties("--platform=%(fullPlatform)s"), WithProperties("--%(configuration)s"), "extract"]
    name = "extract-built-product"
    description = ["extracting built product"]
    descriptionDone = ["extracted built product"]
    haltOnFailure = True


class UploadBuiltProduct(transfer.FileUpload):
    name = 'upload-built-product'
    workersrc = WithProperties("WebKitBuild/%(configuration)s.zip")
    masterdest = WithProperties("archives/%(fullPlatform)s-%(architecture)s-%(configuration)s/%(archive_revision)s.zip")
    haltOnFailure = True

    def __init__(self, **kwargs):
        kwargs['workersrc'] = self.workersrc
        kwargs['masterdest'] = self.masterdest
        kwargs['mode'] = 0o644
        kwargs['blocksize'] = 1024 * 256
        transfer.FileUpload.__init__(self, **kwargs)


class UploadMinifiedBuiltProduct(UploadBuiltProduct):
    name = 'upload-minified-built-product'
    workersrc = WithProperties("WebKitBuild/minified-%(configuration)s.zip")
    masterdest = WithProperties("archives/%(fullPlatform)s-%(architecture)s-%(configuration)s/minified-%(archive_revision)s.zip")


class DownloadBuiltProduct(shell.ShellCommandNewStyle):
    command = ["python3", "Tools/CISupport/download-built-product",
        WithProperties("--platform=%(platform)s"), WithProperties("--%(configuration)s"),
        WithProperties(S3URL + "archives.webkit.org/%(fullPlatform)s-%(architecture)s-%(configuration)s/%(archive_revision)s.zip")]
    name = "download-built-product"
    description = ["downloading built product"]
    descriptionDone = ["downloaded built product"]
    haltOnFailure = False
    flunkOnFailure = False

    @defer.inlineCallbacks
    def run(self):
        # Only try to download from S3 on the official deployment <https://webkit.org/b/230006>
        if CURRENT_HOSTNAME != BUILD_WEBKIT_HOSTNAME:
            self.build.addStepsAfterCurrentStep([DownloadBuiltProductFromMaster()])
            defer.returnValue(SKIPPED)

        rc = yield super().run()
        if rc == FAILURE:
            self.build.addStepsAfterCurrentStep([DownloadBuiltProductFromMaster()])
        defer.returnValue(rc)


class DownloadBuiltProductFromMaster(transfer.FileDownload):
    mastersrc = WithProperties('archives/%(fullPlatform)s-%(architecture)s-%(configuration)s/%(archive_revision)s.zip')
    workerdest = WithProperties('WebKitBuild/%(configuration)s.zip')
    name = 'download-built-product-from-master'
    description = ['downloading built product from buildbot master']
    descriptionDone = ['Downloaded built product']
    haltOnFailure = True
    flunkOnFailure = True

    def __init__(self, **kwargs):
        # Allow the unit test to override mastersrc
        if 'mastersrc' not in kwargs:
            kwargs['mastersrc'] = self.mastersrc
        kwargs['workerdest'] = self.workerdest
        kwargs['mode'] = 0o0644
        kwargs['blocksize'] = 1024 * 256
        transfer.FileDownload.__init__(self, **kwargs)

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {u'step': u'Failed to download built product from build master'}
        return super(DownloadBuiltProductFromMaster, self).getResultSummary()


class RunJavaScriptCoreTests(TestWithFailureCount, CustomFlagsMixin):
    name = "jscore-test"
    description = ["jscore-tests running"]
    descriptionDone = ["jscore-tests"]
    jsonFileName = "jsc_results.json"
    command = [
        "perl", "Tools/Scripts/run-javascriptcore-tests",
        "--no-build", "--no-fail-fast",
        "--json-output={0}".format(jsonFileName),
        WithProperties("--%(configuration)s"),
        "--builder-name", WithProperties("%(buildername)s"),
        "--build-number", WithProperties("%(buildnumber)s"),
        "--buildbot-worker", WithProperties("%(workername)s"),
        "--buildbot-master", CURRENT_HOSTNAME,
        "--report", RESULTS_WEBKIT_URL,
    ]
    commandExtra = ['--treat-failing-as-flaky=0.7,10,20']
    failedTestsFormatString = "%d JSC test%s failed"
    logfiles = {"json": jsonFileName}

    def __init__(self, *args, **kwargs):
        kwargs['logEnviron'] = False
        if 'sigtermTime' not in kwargs:
            kwargs['sigtermTime'] = 10
        TestWithFailureCount.__init__(self, *args, **kwargs)

    def start(self):
        self.workerEnvironment[RESULTS_SERVER_API_KEY] = os.getenv(RESULTS_SERVER_API_KEY)
        self.log_observer = logobserver.BufferLogObserver()
        self.addLogObserver('stdio', self.log_observer)
        self.failedTestCount = 0

        platform = self.getProperty('platform')
        architecture = self.getProperty("architecture")
        # Ask run-jsc-stress-tests to report flaky (including to the resultsdb)
        # but do not fail the whole run if the pass rate of the failing tests is
        # high enough.
        self.command += self.commandExtra
        # Currently run-javascriptcore-test doesn't support run javascript core test binaries list below remotely
        if architecture in ['aarch64'] or platform in ['win']:
            self.command += ['--no-testmasm', '--no-testair', '--no-testb3', '--no-testdfg', '--no-testapi']
        # Linux bots have currently problems with JSC tests that try to use large amounts of memory.
        # Check: https://bugs.webkit.org/show_bug.cgi?id=175140
        if platform in ('gtk', 'wpe', 'jsc-only'):
            self.setCommand(self.command + ['--memory-limited', '--verbose'])
        # WinCairo uses the Windows command prompt, not Cygwin.
        elif platform == 'wincairo':
            self.setCommand(self.command + ['--test-writer=ruby'])

        self.appendCustomBuildFlags(platform, self.getProperty('fullPlatform'))
        return shell.Test.start(self)

    def countFailures(self, cmd):
        logText = self.log_observer.getStdout()
        count = 0

        match = re.search(r'^Results for JSC stress tests:\r?\n\s+(\d+) failure', logText, re.MULTILINE)
        if match:
            count += int(match.group(1))

        match = re.search(r'Results for JSC test binaries:\r?\n\s+(\d+) failure', logText, re.MULTILINE)
        if match:
            count += int(match.group(1))

        match = re.search(r'^Results for Mozilla tests:\r?\n\s+(\d+) regression', logText, re.MULTILINE)
        if match:
            count += int(match.group(1))

        return count


class RunTest262Tests(TestWithFailureCount, CustomFlagsMixin):
    name = "test262-test"
    description = ["test262-tests running"]
    descriptionDone = ["test262-tests"]
    failedTestsFormatString = "%d Test262 test%s failed"
    command = ["perl", "Tools/Scripts/test262-runner", "--verbose", WithProperties("--%(configuration)s")]
    test_summary_re = re.compile(r'^\! NEW FAIL')

    def start(self):
        self.log_observer = ParseByLineLogObserver(self.parseOutputLine)
        self.addLogObserver('stdio', self.log_observer)
        self.failedTestCount = 0
        self.appendCustomBuildFlags(self.getProperty('platform'), self.getProperty('fullPlatform'))
        return shell.Test.start(self)

    def parseOutputLine(self, line):
        match = self.test_summary_re.match(line)
        if match:
            self.failedTestCount += 1

    def countFailures(self, cmd):
        return self.failedTestCount


class RunWebKitTests(shell.Test, CustomFlagsMixin):
    name = "layout-test"
    description = ["layout-tests running"]
    descriptionDone = ["layout-tests"]
    resultDirectory = "layout-test-results"
    command = ["python3", "Tools/Scripts/run-webkit-tests",
               "--no-build",
               "--no-show-results",
               "--no-new-test-results",
               "--clobber-old-results",
               "--builder-name", WithProperties("%(buildername)s"),
               "--build-number", WithProperties("%(buildnumber)s"),
               "--buildbot-worker", WithProperties("%(workername)s"),
               "--buildbot-master", CURRENT_HOSTNAME,
               "--report", RESULTS_WEBKIT_URL,
               "--exit-after-n-crashes-or-timeouts", "50",
               "--exit-after-n-failures", "500",
               WithProperties("--%(configuration)s")]

    # FIXME: This will break if run-webkit-tests changes its default log formatter.
    nrwt_log_message_regexp = re.compile(r'\d{2}:\d{2}:\d{2}(\.\d+)?\s+\d+\s+(?P<message>.*)')
    expressions = [
        ('flakes', re.compile(r'Unexpected flakiness.+\((\d+)\)')),
        ('new passes', re.compile(r'Expected to .+, but passed:\s+\((\d+)\)')),
        ('missing results', re.compile(r'Regressions: Unexpected missing results\s+\((\d+)\)')),
        ('failures', re.compile(r'Regressions: Unexpected.+\((\d+)\)')),
    ]
    cancelled_due_to_huge_logs = False
    line_count = 0

    def __init__(self, *args, **kwargs):
        kwargs['logEnviron'] = False
        shell.Test.__init__(self, *args, **kwargs)

    def start(self):
        self.workerEnvironment[RESULTS_SERVER_API_KEY] = os.getenv(RESULTS_SERVER_API_KEY)
        self.log_observer = ParseByLineLogObserver(self.parseOutputLine)
        self.addLogObserver('stdio', self.log_observer)
        self.incorrectLayoutLines = []
        self.testFailures = {}

        platform = self.getProperty('platform')
        self.appendCustomTestingFlags(platform, self.getProperty('device_model'))
        additionalArguments = self.getProperty('additionalArguments')

        self.setCommand(self.command + ["--results-directory", self.resultDirectory])
        self.setCommand(self.command + ['--debug-rwt-logging'])

        if platform == "win":
            self.setCommand(self.command + ['--batch-size', '100', '--root=' + os.path.join("WebKitBuild", self.getProperty('configuration'), "bin64")])

        if platform in ['gtk', 'wpe']:
            self.setCommand(self.command + ['--enable-core-dumps-nolimit'])

        if additionalArguments:
            self.setCommand(self.command + additionalArguments)
        return shell.Test.start(self)

    def _strip_python_logging_prefix(self, line):
        match_object = self.nrwt_log_message_regexp.match(line)
        if match_object:
            return match_object.group('message')
        return line

    def parseOutputLine(self, line):
        self.line_count += 1
        if self.line_count == THRESHOLD_FOR_EXCESSIVE_LOGS:
            self.handleExcessiveLogging()
            return

        if r'Exiting early' in line or r'leaks found' in line:
            self.incorrectLayoutLines.append(self._strip_python_logging_prefix(line))
            return

        for name, expression in self.expressions:
            match = expression.search(line)
            if match:
                self.testFailures[name] = self.testFailures.get(name, 0) + int(match.group(1))

    def processTestFailures(self):
        for name, result in self.testFailures.items():
            self.incorrectLayoutLines.append(str(result) + ' ' + name)

    def evaluateCommand(self, cmd):
        self.processTestFailures()
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

    def getResultSummary(self):
        if self.cancelled_due_to_huge_logs:
            return {'step': MSG_FOR_EXCESSIVE_LOGS, 'build': MSG_FOR_EXCESSIVE_LOGS}
        if self.results != SUCCESS and self.incorrectLayoutLines:
            return {'step': ' '.join(self.incorrectLayoutLines)}
        return super().getResultSummary()

    def handleExcessiveLogging(self):
        build_url = f'{self.master.config.buildbotURL}#/builders/{self.build._builderid}/builds/{self.build.number}'
        print(f'\n{MSG_FOR_EXCESSIVE_LOGS}, {build_url}\n')
        self.cancelled_due_to_huge_logs = True
        self.build.stopBuild(reason=MSG_FOR_EXCESSIVE_LOGS, results=FAILURE)
        self.build.buildFinished([MSG_FOR_EXCESSIVE_LOGS], FAILURE)


class RunDashboardTests(RunWebKitTests):
    name = "dashboard-tests"
    description = ["dashboard-tests running"]
    descriptionDone = ["dashboard-tests"]
    resultDirectory = os.path.join(RunWebKitTests.resultDirectory, "dashboard-layout-test-results")

    def start(self):
        self.setCommand(self.command + ["--layout-tests-directory", "Tools/CISupport/build-webkit-org/public_html/dashboard/Scripts/tests"])
        return RunWebKitTests.start(self)


class RunAPITests(TestWithFailureCount, CustomFlagsMixin):
    name = "run-api-tests"
    VALID_ADDITIONAL_ARGUMENTS_LIST = ["--remote-layer-tree", "--use-gpu-process"]
    description = ["api tests running"]
    descriptionDone = ["api-tests"]
    jsonFileName = "api_test_results.json"
    logfiles = {"json": jsonFileName}
    command = [
        "python3",
        "Tools/Scripts/run-api-tests",
        "--no-build",
        "--json-output={0}".format(jsonFileName),
        WithProperties("--%(configuration)s"),
        "--verbose",
        "--buildbot-master", CURRENT_HOSTNAME,
        "--builder-name", WithProperties("%(buildername)s"),
        "--build-number", WithProperties("%(buildnumber)s"),
        "--buildbot-worker", WithProperties("%(workername)s"),
        "--report", RESULTS_WEBKIT_URL,
    ]
    failedTestsFormatString = "%d api test%s failed or timed out"
    test_summary_re = re.compile(r'Ran (?P<ran>\d+) tests of (?P<total>\d+) with (?P<passed>\d+) successful')
    cancelled_due_to_huge_logs = False
    line_count = 0

    def __init__(self, *args, **kwargs):
        kwargs['logEnviron'] = False
        TestWithFailureCount.__init__(self, *args, **kwargs)

    def start(self):
        self.workerEnvironment[RESULTS_SERVER_API_KEY] = os.getenv(RESULTS_SERVER_API_KEY)
        self.log_observer = ParseByLineLogObserver(self.parseOutputLine)
        self.addLogObserver('stdio', self.log_observer)
        self.failedTestCount = 0
        self.appendCustomTestingFlags(self.getProperty('platform'), self.getProperty('device_model'))
        additionalArguments = self.getProperty("additionalArguments")
        for additionalArgument in additionalArguments or []:
            if additionalArgument in self.VALID_ADDITIONAL_ARGUMENTS_LIST:
                self.command += [additionalArgument]
        return shell.Test.start(self)

    def countFailures(self, cmd):
        return self.failedTestCount

    def parseOutputLine(self, line):
        self.line_count += 1
        if self.line_count == THRESHOLD_FOR_EXCESSIVE_LOGS:
            self.handleExcessiveLogging()
            return

        match = self.test_summary_re.match(line)
        if match:
            self.failedTestCount = int(match.group('ran')) - int(match.group('passed'))

    def handleExcessiveLogging(self):
        build_url = f'{self.master.config.buildbotURL}#/builders/{self.build._builderid}/builds/{self.build.number}'
        print(f'\n{MSG_FOR_EXCESSIVE_LOGS}, {build_url}\n')
        self.cancelled_due_to_huge_logs = True
        self.build.stopBuild(reason=MSG_FOR_EXCESSIVE_LOGS, results=FAILURE)
        self.build.buildFinished([MSG_FOR_EXCESSIVE_LOGS], FAILURE)

    def getResultSummary(self):
        if self.cancelled_due_to_huge_logs:
            return {'step': MSG_FOR_EXCESSIVE_LOGS, 'build': MSG_FOR_EXCESSIVE_LOGS}
        return super().getResultSummary()


class RunPythonTests(TestWithFailureCount):
    test_summary_re = re.compile(r'^FAILED \((?P<counts>[^)]+)\)')  # e.g.: FAILED (failures=2, errors=1)

    def start(self):
        self.log_observer = ParseByLineLogObserver(self.parseOutputLine)
        self.addLogObserver('stdio', self.log_observer)
        self.failedTestCount = 0
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

    def parseOutputLine(self, line):
        match = self.test_summary_re.match(line)
        if match:
            self.failedTestCount = sum(int(component.split('=')[1]) for component in match.group('counts').split(', '))

    def countFailures(self, cmd):
        return self.failedTestCount


class RunWebKitPyTests(RunPythonTests):
    name = "webkitpy-test"
    description = ["python-tests running"]
    descriptionDone = ["python-tests"]
    command = [
        "python3",
        "Tools/Scripts/test-webkitpy",
        "--verbose",
        "--buildbot-master", CURRENT_HOSTNAME,
        "--builder-name", WithProperties("%(buildername)s"),
        "--build-number", WithProperties("%(buildnumber)s"),
        "--buildbot-worker", WithProperties("%(workername)s"),
        "--report", RESULTS_WEBKIT_URL,
    ]
    failedTestsFormatString = "%d python test%s failed"

    def __init__(self, *args, **kwargs):
        kwargs['logEnviron'] = False
        RunPythonTests.__init__(self, *args, **kwargs)

    def start(self):
        self.workerEnvironment[RESULTS_SERVER_API_KEY] = os.getenv(RESULTS_SERVER_API_KEY)
        return RunPythonTests.start(self)


class RunLLDBWebKitTests(RunPythonTests):
    name = "lldb-webkit-test"
    description = ["lldb-webkit-tests running"]
    descriptionDone = ["lldb-webkit-tests"]
    command = [
        "python3",
        "Tools/Scripts/test-lldb-webkit",
        "--verbose",
        "--no-build",
        WithProperties("--%(configuration)s"),
    ]
    failedTestsFormatString = "%d lldb test%s failed"


class RunPerlTests(TestWithFailureCount):
    name = "webkitperl-test"
    description = ["perl-tests running"]
    descriptionDone = ["perl-tests"]
    command = ["perl", "Tools/Scripts/test-webkitperl"]
    failedTestsFormatString = "%d perl test%s failed"
    test_summary_re = re.compile(r'^Failed \d+/\d+ test programs\. (?P<count>\d+)/\d+ subtests failed\.')  # e.g.: Failed 2/19 test programs. 5/363 subtests failed.

    def start(self):
        self.log_observer = ParseByLineLogObserver(self.parseOutputLine)
        self.addLogObserver('stdio', self.log_observer)
        self.failedTestCount = 0
        return shell.Test.start(self)

    def parseOutputLine(self, line):
        match = self.test_summary_re.match(line)
        if match:
            self.failedTestCount = int(match.group('count'))

    def countFailures(self, cmd):
        return self.failedTestCount


class RunLLINTCLoopTests(TestWithFailureCount):
    name = "webkit-jsc-cloop-test"
    description = ["cloop-tests running"]
    descriptionDone = ["cloop-tests"]
    jsonFileName = "jsc_cloop.json"
    command = [
        "perl", "Tools/Scripts/run-javascriptcore-tests",
        "--no-build",
        "--no-jsc-stress", "--no-fail-fast",
        "--json-output={0}".format(jsonFileName),
        WithProperties("--%(configuration)s"),
        "--builder-name", WithProperties("%(buildername)s"),
        "--build-number", WithProperties("%(buildnumber)s"),
        "--buildbot-worker", WithProperties("%(workername)s"),
        "--buildbot-master", CURRENT_HOSTNAME,
        "--report", RESULTS_WEBKIT_URL,
    ]
    failedTestsFormatString = "%d regression%s found."
    logfiles = {"json": jsonFileName}
    test_summary_re = re.compile(r'\s*(?P<count>\d+) regressions? found.')  # e.g.: 2 regressions found.

    def __init__(self, *args, **kwargs):
        kwargs['logEnviron'] = False
        TestWithFailureCount.__init__(self, *args, **kwargs)

    def start(self):
        self.workerEnvironment[RESULTS_SERVER_API_KEY] = os.getenv(RESULTS_SERVER_API_KEY)
        self.log_observer = ParseByLineLogObserver(self.parseOutputLine)
        self.addLogObserver('stdio', self.log_observer)
        self.failedTestCount = 0
        return shell.Test.start(self)

    def parseOutputLine(self, line):
        match = self.test_summary_re.match(line)
        if match:
            self.failedTestCount = int(match.group('count'))

    def countFailures(self, cmd):
        return self.failedTestCount


class Run32bitJSCTests(TestWithFailureCount):
    name = "webkit-32bit-jsc-test"
    description = ["32bit-jsc-tests running"]
    descriptionDone = ["32bit-jsc-tests"]
    jsonFileName = "jsc_32bit.json"
    command = [
        "perl", "Tools/Scripts/run-javascriptcore-tests",
        "--32-bit", "--no-build",
        "--no-fail-fast", "--no-jit", "--no-testair", "--no-testb3", "--no-testmasm",
        "--json-output={0}".format(jsonFileName),
        WithProperties("--%(configuration)s"),
        "--builder-name", WithProperties("%(buildername)s"),
        "--build-number", WithProperties("%(buildnumber)s"),
        "--buildbot-worker", WithProperties("%(workername)s"),
        "--buildbot-master", CURRENT_HOSTNAME,
        "--report", RESULTS_WEBKIT_URL,
    ]
    failedTestsFormatString = "%d regression%s found."
    logfiles = {"json": jsonFileName}
    test_summary_re = re.compile(r'\s*(?P<count>\d+) failures? found.')  # e.g.: 2 failures found.

    def __init__(self, *args, **kwargs):
        kwargs['logEnviron'] = False
        TestWithFailureCount.__init__(self, *args, **kwargs)

    def start(self):
        self.workerEnvironment[RESULTS_SERVER_API_KEY] = os.getenv(RESULTS_SERVER_API_KEY)
        self.log_observer = ParseByLineLogObserver(self.parseOutputLine)
        self.addLogObserver('stdio', self.log_observer)
        self.failedTestCount = 0
        return shell.Test.start(self)

    def parseOutputLine(self, line):
        match = self.test_summary_re.match(line)
        if match:
            self.failedTestCount = int(match.group('count'))

    def countFailures(self, cmd):
        return self.failedTestCount


class RunBindingsTests(shell.Test):
    name = "bindings-generation-tests"
    description = ["bindings-tests running"]
    descriptionDone = ["bindings-tests"]
    command = ["python3", "Tools/Scripts/run-bindings-tests"]


class RunBuiltinsTests(shell.Test):
    name = "builtins-generator-tests"
    description = ["builtins-generator-tests running"]
    descriptionDone = ["builtins-generator-tests"]
    command = ["python3", "Tools/Scripts/run-builtins-generator-tests"]


class RunGLibAPITests(shell.Test):
    name = "API-tests"
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

        foundItems = re.findall(r"Unexpected failures \((\d+)\)", logText)
        if foundItems:
            failedTests = int(foundItems[0])
            messages.append("%d failures" % failedTests)

        foundItems = re.findall(r"Unexpected crashes \((\d+)\)", logText)
        if foundItems:
            crashedTests = int(foundItems[0])
            messages.append("%d crashes" % crashedTests)

        foundItems = re.findall(r"Unexpected timeouts \((\d+)\)", logText)
        if foundItems:
            timedOutTests = int(foundItems[0])
            messages.append("%d timeouts" % timedOutTests)

        foundItems = re.findall(r"Unexpected passes \((\d+)\)", logText)
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
    command = ["python3", "Tools/Scripts/run-gtk-tests", WithProperties("--%(configuration)s")]


class RunWPEAPITests(RunGLibAPITests):
    command = ["python3", "Tools/Scripts/run-wpe-tests", WithProperties("--%(configuration)s")]


class RunWebDriverTests(shell.Test, CustomFlagsMixin):
    name = "webdriver-test"
    description = ["webdriver-tests running"]
    descriptionDone = ["webdriver-tests"]
    jsonFileName = "webdriver_tests.json"
    command = ["python3", "Tools/Scripts/run-webdriver-tests", "--json-output={0}".format(jsonFileName), WithProperties("--%(configuration)s")]
    logfiles = {"json": jsonFileName}
    cancelled_due_to_huge_logs = False
    line_count = 0

    def start(self):
        additionalArguments = self.getProperty('additionalArguments')
        if additionalArguments:
            self.setCommand(self.command + additionalArguments)

        self.log_observer = ParseByLineLogObserver(self.parseOutputLine)
        self.addLogObserver('stdio', self.log_observer)

        self.appendCustomBuildFlags(self.getProperty('platform'), self.getProperty('fullPlatform'))
        return shell.Test.start(self)

    def getResultSummary(self):
        # TODO: Parse logs to count number of failures and unexpected passes. https://webkit.org/b/261698
        if self.cancelled_due_to_huge_logs:
            return {'step': MSG_FOR_EXCESSIVE_LOGS, 'build': MSG_FOR_EXCESSIVE_LOGS}
        if self.results == FAILURE:
            return {'step': f'Failed {self.name}'}
        return super().getResultSummary()

    def parseOutputLine(self, line):
        self.line_count += 1
        if self.line_count == THRESHOLD_FOR_EXCESSIVE_LOGS:
            self.handleExcessiveLogging()
            return

    def handleExcessiveLogging(self):
        build_url = f'{self.master.config.buildbotURL}#/builders/{self.build._builderid}/builds/{self.build.number}'
        print(f'\n{MSG_FOR_EXCESSIVE_LOGS}, {build_url}\n')
        self.cancelled_due_to_huge_logs = True
        self.build.stopBuild(reason=MSG_FOR_EXCESSIVE_LOGS, results=FAILURE)
        self.build.buildFinished([MSG_FOR_EXCESSIVE_LOGS], FAILURE)


class RunWebKit1Tests(RunWebKitTests):
    def start(self):
        self.setCommand(self.command + ["--dump-render-tree"])

        return RunWebKitTests.start(self)


class RunWebKit1LeakTests(RunWebKit1Tests):
    want_stdout = False
    want_stderr = False
    warnOnWarnings = True

    def start(self):
        self.setCommand(self.command + ["--leaks", "--result-report-flavor", "Leaks"])
        return RunWebKit1Tests.start(self)


class RunAndUploadPerfTests(shell.Test):
    name = "perf-test"
    description = ["perf-tests running"]
    descriptionDone = ["perf-tests"]
    command = ["python3", "Tools/Scripts/run-perf-tests",
               "--output-json-path", "perf-test-results.json",
               "--worker-config-json-path", "../../perf-test-config.json",
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
                return ["worker config JSON error"]
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
    command = ["python3", "Tools/Scripts/browserperfdash-benchmark", "--plans-from-config",
               "--config-file", "../../browserperfdash-benchmark-config.txt",
               "--browser-version", WithProperties("%(archive_revision)s"),
               "--timestamp-from-repo", "."]

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


class ArchiveTestResults(shell.ShellCommandNewStyle):
    command = ["python3", "Tools/CISupport/test-result-archive",
               WithProperties("--platform=%(platform)s"), WithProperties("--%(configuration)s"), "archive"]
    name = "archive-test-results"
    description = ["archiving test results"]
    descriptionDone = ["archived test results"]
    haltOnFailure = True


class UploadTestResults(transfer.FileUpload):
    workersrc = "layout-test-results.zip"
    masterdest = WithProperties("public_html/results/%(buildername)s/%(archive_revision)s (%(buildnumber)s).zip")

    def __init__(self, **kwargs):
        kwargs['workersrc'] = self.workersrc
        kwargs['masterdest'] = self.masterdest
        kwargs['mode'] = 0o644
        kwargs['blocksize'] = 1024 * 256
        transfer.FileUpload.__init__(self, **kwargs)


class TransferToS3(master.MasterShellCommandNewStyle):
    name = "transfer-to-s3"
    description = ["transferring to s3"]
    descriptionDone = ["transferred to s3"]
    archive = WithProperties("archives/%(fullPlatform)s-%(architecture)s-%(configuration)s/%(archive_revision)s.zip")
    minifiedArchive = WithProperties("archives/%(fullPlatform)s-%(architecture)s-%(configuration)s/minified-%(archive_revision)s.zip")
    identifier = WithProperties("%(fullPlatform)s-%(architecture)s-%(configuration)s")
    revision = WithProperties("%(archive_revision)s")
    command = ["python3", "../Shared/transfer-archive-to-s3", "--revision", revision, "--identifier", identifier, "--archive", archive]
    haltOnFailure = True

    def __init__(self, terminate_build=False, **kwargs):
        kwargs['command'] = self.command
        kwargs['logEnviron'] = False
        self.terminate_build = terminate_build
        super().__init__(**kwargs)

    @defer.inlineCallbacks
    def run(self):
        rc = yield super().run()

        if self.terminate_build and self.getProperty('user_provided_git_hash'):
            self.build.buildFinished([f"Uploaded archive with hash {self.getProperty('user_provided_git_hash', '')[:8]}"], SUCCESS)
        defer.returnValue(rc)

    def doStepIf(self, step):
        return CURRENT_HOSTNAME == BUILD_WEBKIT_HOSTNAME


class ExtractTestResults(master.MasterShellCommandNewStyle):
    name = 'extract-test-results'
    descriptionDone = ['Extracted test results']
    renderables = ['resultDirectory', 'zipFile']

    def __init__(self, **kwargs):
        kwargs['command'] = ""
        kwargs['logEnviron'] = False
        self.zipFile = Interpolate('public_html/results/%(prop:buildername)s/%(prop:archive_revision)s (%(prop:buildnumber)s).zip')
        self.resultDirectory = Interpolate('public_html/results/%(prop:buildername)s/%(prop:archive_revision)s (%(prop:buildnumber)s)')
        kwargs['command'] = ['unzip', '-q', '-o', self.zipFile, '-d', self.resultDirectory]
        super().__init__(**kwargs)

    def resultDirectoryURL(self):
        self.setProperty('result_directory', self.resultDirectory)
        return self.resultDirectory.replace('public_html/', '/') + '/'

    def addCustomURLs(self):
        self.addURL("view layout test results", self.resultDirectoryURL() + "results.html")
        self.addURL("view dashboard test results", self.resultDirectoryURL() + "dashboard-layout-test-results/results.html")

    @defer.inlineCallbacks
    def run(self):
        rc = yield super().run()
        self.addCustomURLs()
        defer.returnValue(rc)


class PrintConfiguration(steps.ShellSequence):
    name = 'configuration'
    description = ['configuration']
    haltOnFailure = False
    flunkOnFailure = False
    warnOnFailure = False
    logEnviron = False
    command_list_generic = [['hostname']]
    command_list_apple = [['df', '-hl'], ['date'], ['sw_vers'], ['system_profiler', 'SPSoftwareDataType', 'SPHardwareDataType'], ['/bin/sh', '-c', 'echo TimezoneVers: $(cat /usr/share/zoneinfo/+VERSION)'], ['xcodebuild', '-sdk', '-version']]
    command_list_linux = [['df', '-hl', '--exclude-type=fuse.portal'], ['date'], ['uname', '-a'], ['uptime']]
    command_list_win = [['df', '-hl']]

    def __init__(self, **kwargs):
        super(PrintConfiguration, self).__init__(timeout=60, **kwargs)
        self.commands = []
        self.log_observer = logobserver.BufferLogObserver(wantStderr=True)
        self.addLogObserver('stdio', self.log_observer)

    def run(self):
        command_list = list(self.command_list_generic)
        platform = self.getProperty('platform', '*')
        if platform != 'jsc-only':
            platform = platform.split('-')[0]
        if platform in ('mac', 'ios', 'tvos', 'watchos', '*'):
            command_list.extend(self.command_list_apple)
        elif platform in ('gtk', 'wpe', 'jsc-only'):
            command_list.extend(self.command_list_linux)
        elif platform in ('win'):
            command_list.extend(self.command_list_win)

        for command in command_list:
            self.commands.append(util.ShellArg(command=command, logname='stdio'))
        return super(PrintConfiguration, self).run()

    def convert_build_to_os_name(self, build):
        if not build:
            return 'Unknown'

        build_to_name_mapping = {
            '12': 'Monterey',
            '11': 'Big Sur',
            '10.15': 'Catalina',
        }

        for key, value in build_to_name_mapping.items():
            if build.startswith(key):
                return value
        return 'Unknown'

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {'step': 'Failed to print configuration'}
        logText = self.log_observer.getStdout() + self.log_observer.getStderr()
        configuration = 'Printed configuration'
        match = re.search('ProductVersion:[ \t]*(.+?)\n', logText)
        if match:
            os_version = match.group(1).strip()
            os_name = self.convert_build_to_os_name(os_version)
            configuration = 'OS: {} ({})'.format(os_name, os_version)

        xcode_re = sdk_re = 'Xcode[ \t]+?([0-9.]+?)\n'
        match = re.search(xcode_re, logText)
        if match:
            xcode_version = match.group(1).strip()
            configuration += ', Xcode: {}'.format(xcode_version)
        return {'step': configuration}


class SetPermissions(master.MasterShellCommandNewStyle):
    name = 'set-permissions'

    def __init__(self, **kwargs):
        resultDirectory = Interpolate('%(prop:result_directory)s')
        kwargs['command'] = ['chmod', 'a+rx', resultDirectory]
        kwargs['logEnviron'] = False
        super().__init__(**kwargs)


class ShowIdentifier(shell.ShellCommandNewStyle):
    name = 'show-identifier'
    identifier_re = '^Identifier: (.*)$'
    flunkOnFailure = False
    haltOnFailure = False

    def __init__(self, **kwargs):
        super().__init__(timeout=10 * 60, logEnviron=False, **kwargs)

    @defer.inlineCallbacks
    def run(self):
        self.log_observer = logobserver.BufferLogObserver()
        self.addLogObserver('stdio', self.log_observer)
        revision = self.getProperty('user_provided_git_hash', None) or self.getProperty('got_revision')
        self.command = ['python3', 'Tools/Scripts/git-webkit', 'find', revision]

        rc = yield super().run()
        if rc != SUCCESS:
            return rc

        log_text = self.log_observer.getStdout()
        match = re.search(self.identifier_re, log_text, re.MULTILINE)
        if match:
            identifier = match.group(1)
            if identifier:
                identifier = identifier.replace('trunk', 'main')
            self.setProperty('identifier', identifier)
            self.setProperty('archive_revision', identifier)

            if self.getProperty('user_provided_git_hash'):
                step = self.getLastBuildStepByName(CheckOutSpecificRevision.name)
            else:
                step = self.getLastBuildStepByName(CheckOutSource.name)

            if not step:
                step = self
            step.addURL('Updated to {}'.format(identifier), self.url_for_identifier(identifier))
            self.descriptionDone = 'Identifier: {}'.format(identifier)
        else:
            self.descriptionDone = 'Failed to find identifier'
            self.setProperty('archive_revision', self.getProperty('got_revision'))
        return rc

    def getLastBuildStepByName(self, name):
        for step in reversed(self.build.executedSteps):
            if name in step.name:
                return step
        return None

    def url_for_identifier(self, identifier):
        return '{}{}'.format(COMMITS_INFO_URL, identifier)

    def getResultSummary(self):
        if self.results != SUCCESS:
            return {u'step': u'Failed to find identifier'}
        return super().getResultSummary()

    def hideStepIf(self, results, step):
        return results == SUCCESS


class CheckIfNeededUpdateDeployedCrossTargetImage(shell.ShellCommandNewStyle, CustomFlagsMixin):
    command = ["python3", "Tools/Scripts/cross-toolchain-helper", "--check-if-image-is-updated", "deployed"]
    name = "check-if-deployed-cross-target-image-is-updated"
    description = ["checking if deployed cross target image is updated"]
    descriptionDone = ["deployed cross target image is updated"]
    haltOnFailure = False
    flunkOnFailure = False
    warnOnFailure = False

    @defer.inlineCallbacks
    def run(self):
        self.appendCrossTargetFlags(self.getProperty('additionalArguments'))
        rc = yield super().run()
        if rc != SUCCESS:
            self.build.addStepsAfterCurrentStep([BuildAndDeployCrossTargetImage()])
        defer.returnValue(rc)


class CheckIfNeededUpdateRunningCrossTargetImage(shell.ShellCommandNewStyle):
    command = ["python3", "Tools/Scripts/cross-toolchain-helper", "--check-if-image-is-updated", "running"]
    name = "check-if-running-cross-target-image-is-updated"
    description = ["checking if running cross target image is updated"]
    descriptionDone = ["running cross target image is updated"]
    haltOnFailure = False
    flunkOnFailure = False
    warnOnFailure = False

    @defer.inlineCallbacks
    def run(self):
        rc = yield super().run()
        if rc != SUCCESS:
            self.build.addStepsAfterCurrentStep([RebootWithUpdatedCrossTargetImage()])
        defer.returnValue(rc)


class BuildAndDeployCrossTargetImage(shell.ShellCommandNewStyle, CustomFlagsMixin):
    command = ["python3", "Tools/Scripts/cross-toolchain-helper", "--build-image",
               "--deploy-image-with-script", "../../cross-toolchain-helper-deploy.sh"]
    name = "build-and-deploy-cross-target-image"
    description = ["building and deploying cross target image"]
    descriptionDone = ["cross target image built and deployed"]
    haltOnFailure = True

    def run(self):
        self.appendCrossTargetFlags(self.getProperty('additionalArguments'))
        return super().run()


class RebootWithUpdatedCrossTargetImage(shell.ShellCommandNewStyle):
    # Either use env var SUDO_ASKPASS or configure /etc/sudoers for passwordless reboot
    command = ["sudo", "-A", "reboot"]
    name = "reboot-with-updated-cross-target-image"
    description = ["rebooting with updated cross target image"]
    descriptionDone = ["rebooted with updated cross target image"]
    haltOnFailure = True

    # This step can never succeed.. either it timeouts (bot has rebooted) or it fails.
    # Ideally buildbot should have built-in reboot support so it waits for the bot to
    # do a reboot, but it doesn't. See: https://github.com/buildbot/buildbot/issues/4578
    @defer.inlineCallbacks
    def run(self):
        rc = yield super().run()
        # The command reboot return success when it instructs the machine to reboot,
        # but the machine can still take a while to stop services and actually reboot.
        # Retry the build if reboot end sucesfully before the connection is lost.
        if rc == SUCCESS:
            self.build.buildFinished(['Rebooting with updated image, retrying build'], RETRY)
        defer.returnValue(rc)
