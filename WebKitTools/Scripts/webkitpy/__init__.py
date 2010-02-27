# Required for Python to search this directory for module files

import os

from init.autoinstall import AutoInstaller

# We put auto-installed third-party modules here:
#
#     webkitpy/thirdparty/autoinstalled
webkitpy_directory = os.path.dirname(__file__)
target_directory = os.path.join(webkitpy_directory, "thirdparty",
                                "autoinstalled")
installer = AutoInstaller(target_dir=target_directory)

# List our third-party library dependencies here and where they can be
# downloaded.
installer.install(url="http://pypi.python.org/packages/source/m/mechanize/mechanize-0.1.11.zip",
                  url_subpath="mechanize")
installer.install(url="http://pypi.python.org/packages/source/C/ClientForm/ClientForm-0.2.10.zip",
                  url_subpath="ClientForm.py")
installer.install(url="http://pypi.python.org/packages/source/p/pep8/pep8-0.5.0.tar.gz#md5=512a818af9979290cd619cce8e9c2e2b",
                  url_subpath="pep8-0.5.0/pep8.py")
