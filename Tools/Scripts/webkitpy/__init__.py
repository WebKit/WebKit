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
import sys

libraries = os.path.join(os.path.abspath(os.path.dirname(os.path.dirname(__file__))), 'libraries')
sys.path.insert(0, os.path.join(libraries, 'webkitcorepy'))

if sys.platform == 'darwin':
    is_root = not os.getuid()
    does_own_libraries = os.stat(libraries).st_uid == os.getuid()
    if (is_root or not does_own_libraries):
        libraries = os.path.expanduser('~/Library/webkitpy')

from webkitcorepy import AutoInstall, Package, Version
AutoInstall.set_directory(os.path.join(libraries, 'autoinstalled', 'python-{}'.format(sys.version_info[0])))

AutoInstall.register(Package('coverage', Version(5, 2, 1)))
AutoInstall.register(Package('mozprocess', Version(1, 2, 0)))
AutoInstall.register(Package('mozlog', Version(6, 1)))
AutoInstall.register(Package('mozterm', Version(1, 0, 0)))
AutoInstall.register(Package('selenium', Version(3, 141, 0)))
AutoInstall.register(Package('toml', Version(0, 10, 1)))

AutoInstall.register(Package('webkitscmpy', Version(0, 0, 1)), local=True)
