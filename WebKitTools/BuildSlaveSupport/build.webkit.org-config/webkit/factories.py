from webkit.steps import *
from buildbot.process import factory

s = factory.s

class BuildFactory(factory.BuildFactory):
    useProgress = False
    def __init__(self):
        factory.BuildFactory.__init__(self, [s(CheckOutSource)])

class StandardBuildFactory(BuildFactory):
    def __init__(self):
        BuildFactory.__init__(self)
        self.steps.append(s(SetConfiguration, configuration="release"))
        self.addCompileStep()
        self.addJavaScriptCoreTestStep()
        self.addLayoutTestStep()
        self.steps.append(s(UploadLayoutResults))
#        self.steps.append(s(UploadDiskImage))

    def addCompileStep(self):
        self.steps.append(s(CompileWebKit, configuration="release"))

    def addJavaScriptCoreTestStep(self):
        self.steps.append(s(JavaScriptCoreTest))

    def addLayoutTestStep(self):
        self.steps.append(s(LayoutTest))


class NoSVGBuildFactory(StandardBuildFactory):
    def addCompileStep(self):
        self.steps.append(s(CompileWebKitNoSVG, configuration="release"))


class PixelTestBuildFactory(BuildFactory):
    def __init__(self):
        BuildFactory.__init__(self)
        self.steps.append(s(SetConfiguration, configuration="release"))
        self.steps.append(s(CompileWebKit, configuration="release"))
        self.steps.append(s(PixelLayoutTest))
        self.steps.append(s(UploadLayoutResults))


class LeakBuildFactory(BuildFactory):
    def __init__(self):
        BuildFactory.__init__(self)
        self.steps.append(s(SetConfiguration, configuration="debug"))
        self.steps.append(s(CompileWebKit, configuration="debug"))
        self.steps.append(s(JavaScriptCoreTest))
        self.steps.append(s(LeakTest))
        self.steps.append(s(UploadLayoutResults))
#        self.steps.append(s(UploadDiskImage))


class PageLoadTestBuildFactory(BuildFactory):
    def __init__(self):
        BuildFactory.__init__(self)
        self.steps.append(s(CompileWebKit, configuration="release"))
        self.steps.append(s(PageLoadTest))


class Win32BuildFactory(StandardBuildFactory):
    def addCompileStep(self):
        self.steps.append(s(InstallWin32Dependencies))
        self.steps.append(s(SetConfiguration, configuration="debug"))
        self.steps.append(s(CompileWebKitWindows, configuration="debug"))

    def addLayoutTestStep(self):
        self.steps.append(s(LayoutTestWindows))

class GtkBuildFactory(StandardBuildFactory):
    def addCompileStep(self):
#        self.steps.append(s(CleanWebKitGtk, configuration="release"))
        self.steps.append(s(CompileWebKitGtk, configuration="release"))

    def addJavaScriptCoreTestStep(self):
        self.steps.append(s(JavaScriptCoreTestGtk))

    def addLayoutTestStep(self):
        pass


class WxBuildFactory(StandardBuildFactory):
    def addCompileStep(self):
        self.steps.append(s(CleanWebKitWx, configuration="release"))
        self.steps.append(s(CompileWebKitWx, configuration="release"))

    def addJavaScriptCoreTestStep(self):
        self.steps.append(s(JavaScriptCoreTestWx))

    def addLayoutTestStep(self):
        pass


class QtBuildFactory(StandardBuildFactory):
    def addCompileStep(self):
        self.steps.append(s(CleanWebKit, configuration="release"))
        self.steps.append(s(CompileWebKit, configuration="release"))

    def addLayoutTestStep(self):
        pass # self.steps.append(s(LayoutTestQt))


class CoverageDataBuildFactory(BuildFactory):
    def __init__(self):
        BuildFactory.__init__(self)
        self.steps.append(s(GenerateCoverageData))
        self.steps.append(s(UploadCoverageData))
