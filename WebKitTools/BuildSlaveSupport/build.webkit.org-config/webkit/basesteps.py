from buildbot.process import step
import os


def buildStepWithDefaultTimeout(klass, default_timeout=45*60):
    class Step(klass):
        timeout = default_timeout
        def __init__(self, *args, **kwargs):
            kwargs['timeout'] = self.timeout
            klass.__init__(self, *args, **kwargs)

    return Step


Test = buildStepWithDefaultTimeout(step.Test)
Compile = buildStepWithDefaultTimeout(step.Compile)
ShellCommand = buildStepWithDefaultTimeout(step.ShellCommand)
SVN = buildStepWithDefaultTimeout(step.SVN)


class UploadCommand:
    def __init__(self, *args, **kwargs):
        self.__build = kwargs['build'].getStatus()
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
        return "/home/buildresults/results/%s/%s/" % (self.getBuild().getBuilder().getName(), self.getBuild().getNumber())

    def getRemotePath(self):
        return "buildresults@build.webkit.org:%s" % (self.getDestinationPath(), )

    def getURLPath(self):
        return '/results/%s/%s/' % (self.getBuild().getBuilder().getName(), self.getBuild().getNumber(), )

    def getBuild(self):
        return self.__build


    def getText(self, cmd, results):
        return self.getText2(cmd, results)

    def getText2(self, cmd, results):
        return ['<a href="%s">%s</a>' % (self.getURLPath(), self.name)]
    
