import os

USE_BUILDBOT_VERSION2 = os.getenv('USE_BUILDBOT_VERSION2') is not None

from twisted.application import service
from buildbot.master import BuildMaster

basedir = '.'
if USE_BUILDBOT_VERSION2:
    configfile = r'master_buildbot2.cfg'
else:
    configfile = r'master.cfg'
rotateLength = 50000000
maxRotatedFiles = 20
umask = 0o022

if basedir == '.':
    basedir = os.path.abspath(os.path.dirname(__file__))

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

