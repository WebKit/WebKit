from webkit.basesteps import ShellCommand, SVN, Test, Compile, UploadCommand
from buildbot.status.builder import SUCCESS, FAILURE, WARNINGS

class CheckOutSource(SVN):
    svnurl = "svn://anoncvs.opensource.apple.com/svn/webkit/trunk"
    mode = "update"
    def __init__(self, *args, **kwargs):
        SVN.__init__(self, svnurl=self.svnurl, mode=self.mode, *args, **kwargs)

class SetConfiguration(ShellCommand):
    command = ["./WebKitTools/Scripts/set-webkit-configuration"]
    
    def __init__(self, *args, **kwargs):
        configuration = kwargs.pop('configuration')
        self.command = self.command + ['--' + configuration]
        self.name = "set-configuration-%s" % (configuration,  )
        self.description = ["set configuration %s" % (configuration, )]
        self.descriptionDone = ["set configuration %s" % (configuration, )]
        ShellCommand.__init__(self, *args, **kwargs)


class LayoutTest(Test):
    name = "layout-test"
    description = ["layout-tests running"]
    descriptionDone = ["layout-tests"]
    command = ["./WebKitTools/Scripts/run-webkit-tests", "--no-launch-safari", "--no-new-test-results", "--results-directory", "layout-test-results"]

    def commandComplete(self, cmd):
        Test.commandComplete(self, cmd)
        
        logText = cmd.logs['stdio'].getText()
        incorrectLayoutLines = [line for line in logText.splitlines() if line.find('had incorrect layout') >= 0]
        if incorrectLayoutLines:
            self.incorrectLayoutLine = incorrectLayoutLines[0]
        else:
            self.incorrectLayoutLine = None

    def getText(self, cmd, results):
        return self.getText2(cmd, results)

    def getText2(self, cmd, results):
        if results != SUCCESS and self.incorrectLayoutLine:
            return [self.incorrectLayoutLine]

        return [self.name]


class JavaScriptCoreTest(Test):
    name = "jscore-test"
    description = ["jscore-tests running"]
    descriptionDone = ["jscore-tests"]
    command = ["./WebKitTools/Scripts/run-javascriptcore-tests"]
    logfiles = {'results': 'JavaScriptCore/tests/mozilla/actual.html'}

    def commandComplete(self, cmd):
        Test.commandComplete(self, cmd)

        logText = cmd.logs['stdio'].getText()
        statusLines = [line for line in logText.splitlines() if line.find('regression') >= 0 and line.find(' found.') >= 0]
        if statusLines and statusLines[0].split()[0] != '0':
            self.regressionLine = statusLines[0]
        else:
            self.regressionLine = None

    def evaluateCommand(self, cmd):
        if cmd.rc != 0:
            return FAILURE

        if self.regressionLine:
            return FAILURE

        return SUCCESS

    def getText(self, cmd, results):
        return self.getText2(cmd, results)

    def getText2(self, cmd, results):
        if results != SUCCESS and self.regressionLine:
            return [self.name, self.regressionLine]

        return [self.name]

class PixelLayoutTest(LayoutTest):
    name = "pixel-layout-test"
    description = ["pixel-layout-tests running"]
    descriptionDone = ["pixel-layout-tests"]
    command = LayoutTest.command + ["--pixel"]

    
class LeakTest(Test):
    name = "leak-test"
    description = ["leak-tests running"]
    descriptionDone = ["leak-tests"]
    command = ["./WebKitTools/Scripts/run-webkit-tests", "--no-launch-safari", "--leaks", "--results-directory", "layout-test-results"]

    def commandComplete(self, cmd):
        Test.commandComplete(self, cmd)

        logText = cmd.logs['stdio'].getText()
        self.totalLeakLines = [line for line in logText.splitlines() if line.find('total leaks found!') >= 0]
        self.totalLeakLines += [line for line in logText.splitlines() if line.find('LEAK: ') >= 0]
        self.totalLeakLines = [' '.join(x.split()[1:]) for x in self.totalLeakLines]

    def evaluateCommand(self, cmd):
        if cmd.rc != 0:
            return FAILURE

        if self.totalLeakLines:
            return FAILURE

        return SUCCESS

    def getText(self, cmd, results):
        return self.getText2(cmd, results)

    def getText2(self, cmd, results):
        if results != SUCCESS and self.totalLeakLines:
            return self.totalLeakLines
        return [self.name]


class PageLoadTest(UploadCommand, Test):
    name = "page-load-test"
    description = ["page-load-tests running"]
    descriptionDone = ["page-load-tests"]
    command = ["./WebKitTools/BuildSlaveSupport/run-performance-tests"]

    def __init__(self, *args, **kwargs):
        UploadCommand.__init__(self, *args, **kwargs)
        self.command = self.command + ['--upload-results', self.getRemotePath()]
        Test.__init__(self, *args, **kwargs)

    def getURLPath(self):
        return UploadCommand.getURLPath(self) + 'PerformanceReportSummary.xml'


class UploadLayoutResults(UploadCommand, ShellCommand):
    name = "upload-results"
    description = ["uploading results"]
    descriptionDone = ["uploaded-results"]

    def __init__(self, *args, **kwargs):
        UploadCommand.__init__(self, *args, **kwargs)
        
        self.command = 'if [[ -d layout-test-results ]]; then find layout-test-results -type d -print0 | xargs -0 chmod ug+rx; find layout-test-results -type f -print0 | xargs -0 chmod ug+r;  rsync -rlvzP --rsync-path="/home/buildresults/bin/rsync" layout-test-results/ %s && rm -rf layout-test-results; fi' % (self.getRemotePath(), )

        ShellCommand.__init__(self, *args, **kwargs)


class CompileWebKit(Compile):
    command = ["./WebKitTools/Scripts/build-webkit", "--no-color"]
    def __init__(self, *args, **kwargs):
        configuration = kwargs.pop('configuration')
        
        self.name = "compile-" + configuration
        self.description = ["compiling " + configuration]
        self.descriptionDone = ["compiled " + configuration]

        Compile.__init__(self, *args, **kwargs)


class CompileWebKitNoSVG(CompileWebKit):
    command = 'rm -rf WebKitBuild && ./WebKitTools/Scripts/build-webkit --no-svg --no-color'

class InstallWin32Dependencies(ShellCommand):
    description = ["installing Windows dependencies"]
    descriptionDone = ["installed Windows dependencies"]
    command = ["WebKitTools/Scripts/install-win-extras"]


class UploadDiskImage(UploadCommand, ShellCommand):
    description = ["uploading disk image"]
    descriptionDone = ["uploaded disk image"]
    name = "upload-disk-image"

    def __init__(self, *args, **kwargs):
        UploadCommand.__init__(self, *args, **kwargs)
        self.command = 'umask 002 && ./WebKitTools/BuildSlaveSupport/build-launcher-app && ./WebKitTools/BuildSlaveSupport/build-launcher-dmg --upload-to-host %s' % (self.getRemotePath(), )
        ShellCommand.__init__(self, *args, **kwargs)
