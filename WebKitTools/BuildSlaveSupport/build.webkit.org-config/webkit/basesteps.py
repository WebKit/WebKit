from buildbot.steps import shell, source
import os


def buildStepWithDefaultTimeout(klass, default_timeout=75*60):
    class Step(klass):
        timeout = default_timeout
        def __init__(self, *args, **kwargs):
            kwargs['timeout'] = self.timeout
            klass.__init__(self, *args, **kwargs)

    return Step


Test = buildStepWithDefaultTimeout(shell.Test)
Compile = buildStepWithDefaultTimeout(shell.Compile)
ShellCommand = buildStepWithDefaultTimeout(shell.ShellCommand)
SVN = buildStepWithDefaultTimeout(source.SVN)


class UploadCommand:
    def initializeForUpload(self):
        try:
            try:
                umask = os.umask(0)
                os.makedirs(self.getDestinationPath(), 042770)
            except OSError, e:
                if e.errno != 17:
                    raise
        finally:
            os.umask(umask)

    def getDestinationPath(self):
        return "/home/buildresults%s" % (self.getURLPath(), )

    def getRemotePath(self):
        return "buildresults@build.webkit.org:%s" % (self.getDestinationPath(), )

    def getURLPath(self):
        return '/results/%s/%s/' % (self.getBuild().builder.name, self.getBuild().getProperty("buildnumber"), )

    def getBuild(self):
        return self.build


    def getText(self, cmd, results):
        return self.getText2(cmd, results)

    def getText2(self, cmd, results):
        return ['<a href="%s">%s</a>' % (self.getURLPath(), self.name)]
    
