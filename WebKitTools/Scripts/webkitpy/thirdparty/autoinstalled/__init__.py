# Required for Python to search this directory for module files

import webkitpy.thirdparty.autoinstall as autoinstall

# List our third-party library dependencies here and where they can be
# downloaded.
autoinstall.bind("ClientForm", "http://pypi.python.org/packages/source/C/ClientForm/ClientForm-0.2.10.zip", "ClientForm-0.2.10")
autoinstall.bind("mechanize", "http://pypi.python.org/packages/source/m/mechanize/mechanize-0.1.11.zip", "mechanize-0.1.11")
# We import both irclib and ircbot because irclib has two top-level packages.
autoinstall.bind("irclib", "http://iweb.dl.sourceforge.net/project/python-irclib/python-irclib/0.4.8/python-irclib-0.4.8.zip", "python-irclib-0.4.8")
autoinstall.bind("ircbot", "http://iweb.dl.sourceforge.net/project/python-irclib/python-irclib/0.4.8/python-irclib-0.4.8.zip", "python-irclib-0.4.8")

from mechanize import Browser
from mechanize import HTTPError
import ircbot
import irclib
