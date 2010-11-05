# Required for Python to search this directory for module files

# This __init__.py makes unit testing easier by allowing us to treat the entire server as one big module.
# This file is only accessed when not on AppEngine itself.

# Make sure that this module will load in that case by including paths to
# the default Google AppEngine install.


def fix_sys_path():
    import sys
    import os

    # AppEngine imports a bunch of google-specific modules.  Thankfully the dev_appserver
    # knows how to do the same.  Re-use the dev_appserver fix_sys_path logic to import
    # all the google.appengine.* stuff so we can run under test-webkitpy
    sys.path.append("/usr/local/google_appengine")
    import dev_appserver
    dev_appserver.fix_sys_path()

    # test-webkitpy adds $WEBKIT/WebKitTools to the sys.path and imports
    # QueueStatusServer to run all the tests.  However, when AppEngine runs
    # our code QueueStatusServer is the root (and thus in the path).
    # Emulate that here for test-webkitpy so that we can import "model."
    # not "QueueStatusServer.model.", etc.
    sys.path.append(os.path.dirname(__file__))


fix_sys_path()
