# Required for Python to search this directory for module files

# Keep this file free of any code or import statements that could
# cause either an error to occur or a log message to be logged.
# This ensures that calling code can import initialization code from
# webkitpy before any errors or log messages due to code in this file.
# Initialization code can include things like version-checking code and
# logging configuration code.
#
# We do not execute any version-checking code or logging configuration
# code in this file so that callers can opt-in as they want.  This also
# allows different callers to choose different initialization code,
# as necessary.

import os
import platform
import sys

# We always want the real system version
os.environ['SYSTEM_VERSION_COMPAT'] = '0'

libraries = os.path.join(os.path.abspath(os.path.dirname(os.path.dirname(__file__))), 'libraries')
webkitcorepy_path = os.path.join(libraries, 'webkitcorepy')
if webkitcorepy_path not in sys.path:
    sys.path.insert(0, webkitcorepy_path)
import webkitcorepy

if sys.platform == 'darwin':
    is_root = not os.getuid()
    does_own_libraries = os.stat(libraries).st_uid == os.getuid()
    if (is_root or not does_own_libraries):
        libraries = os.path.expanduser('~/Library/webkitpy')

from webkitcorepy import AutoInstall, Package, Version
AutoInstall.set_directory(os.path.join(libraries, 'autoinstalled', 'python-{}-{}'.format(sys.version_info[0], platform.machine())))

if sys.version_info >= (3, 7):
    AutoInstall.register(Package('pylint', Version(2, 13, 9)))
    AutoInstall.register(
        Package("pytest", Version(7, 2, 0),
                implicit_deps=["attr", "pluggy", "iniconfig"]
                + (["exceptiongroup"] if sys.version_info < (3, 11) else [])
                + (["importlib_metadata"] if sys.version_info < (3, 8) else [])
                )
    )
    AutoInstall.register(Package('pytest_asyncio', Version(0, 18, 3), pypi_name='pytest-asyncio', implicit_deps=['pytest']))
    AutoInstall.register(Package('pytest_timeout', Version(2, 1, 0), pypi_name='pytest-timeout', implicit_deps=['pytest']))
    AutoInstall.register(Package('websockets', Version(8, 1)))
    if sys.version_info < (3, 11):
        AutoInstall.register(Package('exceptiongroup', Version(1, 1, 0), wheel=True))
elif sys.version_info >= (2, 7) and sys.version_info < (3,):
    AutoInstall.register(Package('pylint', Version(0, 28, 0)))
    AutoInstall.register(Package('logilab.common', Version(0, 58, 1), pypi_name='logilab-common', aliases=['logilab']))
    AutoInstall.register(Package('logilab.astng', Version(0, 24, 1), pypi_name='logilab-astng', aliases=['logilab']))
    AutoInstall.register(Package('pathlib2', Version(2, 3, 5)))
else:
    sys.stderr.write("pytest, pylint and websockets do not support Python version! (%s)\n" % sys.version)

if sys.version_info >= (3, 6):
    AutoInstall.register(Package('importlib_metadata', Version(4, 8, 1)))
    if sys.version_info >= (3, 9):
        AutoInstall.register(Package('typing_extensions', Version(4, 8, 0)))
    else:
        AutoInstall.register(Package('typing_extensions', Version(3, 10, 0)))
else:
    AutoInstall.register(Package('importlib_metadata', Version(1, 7, 0)))

AutoInstall.register(Package('atomicwrites', Version(1, 1, 5)))
AutoInstall.register(Package('attr', Version(21, 3, 0), pypi_name='attrs'))

if sys.version_info >= (3, 6):
    AutoInstall.register(Package('bs4', Version(4, 12, 0), pypi_name='beautifulsoup4'))
else:
    AutoInstall.register(Package('bs4', Version(4, 9, 3), pypi_name='beautifulsoup4'))
AutoInstall.register(Package('configparser', Version(4, 0, 2), implicit_deps=['pyparsing']))
AutoInstall.register(Package('contextlib2', Version(0, 6, 0)))
AutoInstall.register(Package('coverage', Version(5, 2, 1)))
AutoInstall.register(Package('funcsigs', Version(1, 0, 2)))
AutoInstall.register(Package('html5lib', Version(1, 1)))
AutoInstall.register(Package('iniconfig', Version(1, 1, 1)))
AutoInstall.register(Package('mechanize', Version(0, 4, 5)))
AutoInstall.register(Package('more_itertools', Version(4, 2, 0), pypi_name='more-itertools'))
AutoInstall.register(Package('mozprocess', Version(1, 3, 0)))
AutoInstall.register(Package('mozlog', Version(7, 1, 0), wheel=True))
AutoInstall.register(Package('mozterm', Version(1, 0, 0)))
AutoInstall.register(Package('pluggy', Version(0, 13, 1)))
AutoInstall.register(Package('py', Version(1, 11, 0)))
AutoInstall.register(Package('pycodestyle', Version(2, 5, 0)))

if sys.version_info >= (3, 7):
    AutoInstall.register(Package('pyfakefs', Version(5, 2, 4)))
else:
    AutoInstall.register(Package('pyfakefs', Version(3, 7, 2)))

AutoInstall.register(Package('scandir', Version(1, 10, 0)))

if sys.version_info >= (3, 6):
    AutoInstall.register(Package('soupsieve', Version(2, 2, 1)))
else:
    AutoInstall.register(Package('soupsieve', Version(1, 9, 6)))

if sys.version_info < (3, 8):
    AutoInstall.register(Package('selenium', Version(3, 141, 0)))
else:
    AutoInstall.register(Package('selenium', Version(4, 12, 0), wheel=True))

AutoInstall.register(Package('toml', Version(0, 10, 1), implicit_deps=['pyparsing']))
AutoInstall.register(Package('wcwidth', Version(0, 2, 5)))
AutoInstall.register(Package('webencodings', Version(0, 5, 1)))
AutoInstall.register(Package('zipp', Version(1, 2, 0)))
AutoInstall.register(Package('zope.interface', Version(5, 1, 0), aliases=['zope'], pypi_name='zope-interface'))

if sys.version_info > (3, 0):
    AutoInstall.register(Package('reporelaypy', Version(0, 4, 1)), local=True)

AutoInstall.register(Package('webkitflaskpy', Version(0, 3, 0)), local=True)
AutoInstall.register(Package('webkitscmpy', Version(4, 0, 0)), local=True)
AutoInstall.register(Package('webkitbugspy', Version(0, 3, 1)), local=True)

import webkitscmpy
