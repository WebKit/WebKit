
from twisted.application import service
from buildbot.master import BuildMaster

basedir = r'/buildbot/webkit'
configfile = r'master.cfg'
rotateLength = 1000000
maxRotatedFiles = None

application = service.Application('buildmaster')
try:
  from twisted.python.logfile import LogFile
  from twisted.python.log import ILogObserver, FileLogObserver
  logfile = LogFile.fromFullPath("twistd.log", rotateLength=rotateLength,
                                 maxRotatedFiles=maxRotatedFiles)
  application.setComponent(ILogObserver, FileLogObserver(logfile).emit)
except ImportError:
  # probably not yet twisted 8.2.0 and beyond, can't set log yet
  pass
BuildMaster(basedir, configfile).setServiceParent(application)

