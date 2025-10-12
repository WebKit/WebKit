import os

from twisted.application import service
from buildbot.master import BuildMaster

basedir = '.'
configfile = 'master.cfg'
rotateLength = 50000000
maxRotatedFiles = 20
umask = 0o022

if basedir == '.':
    basedir = os.path.abspath(os.path.dirname(__file__))

application = service.Application('buildmaster')
from twisted.python.logfile import LogFile
from twisted.python.log import ILogObserver, FileLogObserver
logfile = LogFile.fromFullPath("twistd.log", rotateLength=rotateLength,
                                maxRotatedFiles=maxRotatedFiles)
application.setComponent(ILogObserver, FileLogObserver(logfile).emit)
BuildMaster(basedir, configfile).setServiceParent(application)
