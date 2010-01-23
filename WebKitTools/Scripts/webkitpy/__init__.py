# Required for Python to search this directory for module files

import autoinstall

# List our third-party library dependencies here and where they can be
# downloaded.
autoinstall.bind("ClientForm", "http://pypi.python.org/packages/source/C/ClientForm/ClientForm-0.2.10.zip", "ClientForm-0.2.10")
autoinstall.bind("mechanize", "http://pypi.python.org/packages/source/m/mechanize/mechanize-0.1.11.zip", "mechanize-0.1.11")
