from webkit.basesteps import ShellCommand, SVN, Test, Compile, UploadCommand
from buildbot.status.builder import SUCCESS, FAILURE, WARNINGS

class CheckOutSource(SVN):
    baseURL = "http://svn.webkit.org/repository/webkit/"
    mode = "update"
    def __init__(self, *args, **kwargs):
        SVN.__init__(self, baseURL=self.baseURL, defaultBranch="trunk", mode=self.mode, *args, **kwargs)

class SetConfiguration(ShellCommand):
    command = ["perl", "./WebKitTools/Scripts/set-webkit-configuration"]
    
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
    command = ["perl", "./WebKitTools/Scripts/run-webkit-tests", "--no-launch-safari", "--no-new-test-results", "--no-sample-on-timeout", "--results-directory", "layout-test-results"]

    def commandComplete(self, cmd):
        Test.commandComplete(self, cmd)
        
        logText = cmd.logs['stdio'].getText()
        incorrectLayoutLines = [line for line in logText.splitlines() if line.find('had incorrect layout') >= 0 or (line.find('test case') >= 0 and (line.find(' crashed') >= 0 or line.find(' timed out') >= 0))]
        self.incorrectLayoutLines = incorrectLayoutLines

    def evaluateCommand(self, cmd):
        if self.incorrectLayoutLines or cmd.rc != 0:
            return FAILURE

        return SUCCESS

    def getText(self, cmd, results):
        return self.getText2(cmd, results)

    def getText2(self, cmd, results):
        if results != SUCCESS and self.incorrectLayoutLines:
            return self.incorrectLayoutLines

        return [self.name]


class JavaScriptCoreTest(Test):
    name = "jscore-test"
    description = ["jscore-tests running"]
    descriptionDone = ["jscore-tests"]
    command = ["perl", "./WebKitTools/Scripts/run-javascriptcore-tests"]
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
        if self.regressionLine:
            return FAILURE

        if cmd.rc != 0:
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
    command = LayoutTest.command + ["--pixel", "--tolerance", "0.1"]

    
class LeakTest(LayoutTest):
    command = ["perl", "./WebKitTools/Scripts/run-webkit-tests", "--no-launch-safari", "--no-sample-on-timeout", "--leaks", "--results-directory", "layout-test-results"]

    def commandComplete(self, cmd):
        LayoutTest.commandComplete(self, cmd)

        logText = cmd.logs['stdio'].getText()
        lines = logText.splitlines()
        leakLines = [line for line in lines if line.find('total leaks found!') >= 0]
        leakLines += [line for line in lines if line.find('LEAK: ') >= 0]
        leakLines = [' '.join(x.split()[1:]) for x in leakLines]

        leakSummary = {}
        for line in leakLines:
            count, key = line.split(' ', 1)
            if key.find('total leaks found!') >= 0:
                key = 'allocations found by "leaks" tool'

            leakSummary[key] = leakSummary.get(key, 0) + int(count)

        leakSummaryLines = []
        for key in sorted(leakSummary.keys()):
            leakSummaryLines.append('%s %s' % (leakSummary[key], key))

        self.incorrectLayoutLines += leakSummaryLines


class UploadLayoutResults(UploadCommand, ShellCommand):
    name = "upload-results"
    description = ["uploading results"]
    descriptionDone = ["uploaded-results"]
    command = "echo Disabled for now"

    def __init__(self, *args, **kwargs):
        ShellCommand.__init__(self, *args, **kwargs)

    def setBuild(self, build):
        ShellCommand.setBuild(self, build)
        self.initializeForUpload()

        self.command = '''
        if [[ -d layout-test-results ]]; then \
            find layout-test-results -type d -print0 | xargs -0 chmod ug+rx; \
            find layout-test-results -type f -print0 | xargs -0 chmod ug+r; \
            rsync -rlvzP --rsync-path=/home/buildresults/bin/rsync layout-test-results/ %s && rm -rf layout-test-results; \
        fi; \
        CRASH_LOG=~/Library/Logs/CrashReporter/DumpRenderTree*.crash*; \
        if [[ -f $(ls -1 $CRASH_LOG | head -n 1 ) ]]; then \
            chmod ug+r $CRASH_LOG; \
            rsync -rlvzP --rsync-path=/home/buildresults/bin/rsync $CRASH_LOG %s && rm -rf $CRASH_LOG; \
        fi; ''' % (self.getRemotePath(), self.getRemotePath())

        self.addFactoryArguments(command=self.command)


class CompileWebKit(Compile):
    command = ["perl", "./WebKitTools/Scripts/build-webkit"]
    env = {'WEBKITSUPPORTLIBRARIESZIPDIR': 'C:\\cygwin\\home\\buildbot', 'MFLAGS':''}
    def __init__(self, *args, **kwargs):
        configuration = kwargs.pop('configuration')
        
        self.name = "compile-" + configuration
        self.description = ["compiling " + configuration]
        self.descriptionDone = ["compiled " + configuration]

        Compile.__init__(self, env=self.env, *args, **kwargs)

class CleanWebKit(CompileWebKit):
    command = CompileWebKit.command + ['--clean']
    description = ['cleaning']
    descriptionDone = ['cleaned']

class CompileWebKitNoSVG(CompileWebKit):
    command = 'rm -rf WebKitBuild && perl ./WebKitTools/Scripts/build-webkit --no-svg'

class CompileWebKitGtk(CompileWebKit):
    command = ['perl', './WebKitTools/Scripts/build-webkit', '--gtk', '--qmake=qmake-qt4']

class CleanWebKitGtk(CompileWebKitGtk):
    command = CompileWebKitGtk.command + ['--clean']
    description = ['cleaning']
    descriptionDone = ['cleaned']

class CompileWebKitWx(CompileWebKit):
    command = ['perl', './WebKitTools/Scripts/build-webkit', '--wx']

class CleanWebKitWx(CompileWebKitWx):
    command = CompileWebKitWx.command + ['--clean']
    description = ['cleaning']
    descriptionDone = ['cleaned']

class CompileWebKitWindows(UploadCommand, CompileWebKit):
    def setBuild(self, build):
        CompileWebKit.setBuild(self, build)
        self.initializeForUpload()

        self.command = '''\
        ./WebKitTools/Scripts/build-webkit; \
        RESULT=$?
        for log in $(find WebKitBuild/*/*/*/*.htm); do \
            chmod ug+r $log; \
            REMOTE_NAME=$(echo $log | sed -e 's|WebKitBuild/obj/||' -e 's|/Release/|-|' -e 's|/Debug/|-|'); \
            rsync -rlvzP --rsync-path="/home/buildresults/bin/rsync" $log %s/$REMOTE_NAME && rm $log; \
        done; \
        exit $RESULT;''' % (self.getRemotePath(), )

        self.addFactoryArguments(command=self.command)

class LayoutTestWindows(LayoutTest):
    env = {'WEBKIT_TESTFONTS': 'C:\\cygwin\\home\\buildbot\\WebKitTestFonts'}

    def __init__(self, *args, **kwargs):
        return LayoutTest.__init__(self, env=self.env, *args, **kwargs)


class JavaScriptCoreTestGtk(JavaScriptCoreTest):
    command = JavaScriptCoreTest.command + ['--gtk']

class JavaScriptCoreTestWx(JavaScriptCoreTest):
    command = JavaScriptCoreTest.command + ['--wx']

class LayoutTestQt(LayoutTest):
    command  = LayoutTest.command + ['--qt']

class InstallWin32Dependencies(ShellCommand):
    description = ["installing Windows dependencies"]
    descriptionDone = ["installed Windows dependencies"]
    command = ["perl", "./WebKitTools/Scripts/update-webkit-auxiliary-libs"]


# class UploadDiskImage(UploadCommand, ShellCommand):
#     description = ["uploading disk image"]
#     descriptionDone = ["uploaded disk image"]
#     name = "upload-disk-image"

#     def __init__(self, *args, **kwargs):
#         UploadCommand.__init__(self, *args, **kwargs)
#         self.command = 'umask 002 && ./WebKitTools/BuildSlaveSupport/build-launcher-app && ./WebKitTools/BuildSlaveSupport/build-launcher-dmg --upload-to-host %s' % (self.getRemotePath(), )
#         ShellCommand.__init__(self, *args, **kwargs)

class GenerateCoverageData(Compile):
    command = ["perl", "./WebKitTools/Scripts/generate-coverage-data"]
    description = ["generating coverage data"]
    descriptionDone = ["generated coverage data"]


class UploadCoverageData(UploadCommand, ShellCommand):
    name = "upload-coverage-data"
    description = ["uploading coverage data"]
    descriptionDone = ["uploaded-coverage-data"]
    command = "echo Disabled for now"

    def __init__(self, *args, **kwargs):
        ShellCommand.__init__(self, *args, **kwargs)

    def setBuild(self, build):
        ShellCommand.setBuild(self, build)
        self.initializeForUpload()
        
        self.command = '''\
        if [[ -d WebKitBuild/Coverage/html ]]; then \
            find WebKitBuild/Coverage/html -type d -print0 | xargs -0 chmod ug+rx; \
            find WebKitBuild/Coverage/html -type f -print0 | xargs -0 chmod ug+r; \
            rsync -rlvzP --rsync-path="/home/buildresults/bin/rsync" WebKitBuild/Coverage/html/ %s && rm -rf WebKitBuild/Coverage/html; \
        fi;''' % (self.getRemotePath(), )

        self.addFactoryArguments(command=self.command)

    def getURLPath(self):
        return "/results/code-coverage/"
