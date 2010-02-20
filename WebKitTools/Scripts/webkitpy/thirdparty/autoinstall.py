# Copyright (c) 2009, Daniel Krech All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#  * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# 
#  * Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
# 
#  * Neither the name of the Daniel Krech nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""\
package loader for auto installing Python packages.

A package loader in the spirit of Zero Install that can be used to
inject dependencies into the import process. 


To install::

    easy_install -U autoinstall

      or 

    download, unpack, python setup.py install

      or 

    try the bootstrap loader. See below.


To use::

    # You can bind any package name to a URL pointing to something
    # that can be imported using the zipimporter.

    autoinstall.bind("pymarc", "http://pypi.python.org/packages/2.5/p/pymarc/pymarc-2.1-py2.5.egg")

    import pymarc

    print pymarc.__version__, pymarc.__file__

    
Changelog::

- added support for non top level packages.
- cache files now use filename part from URL.
- applied patch from Eric Seidel <eseidel@google.com> to add support
for loading modules where the module is not at the root of the .zip
file.


TODO::

- a description of the intended use case
- address other issues pointed out in:

    http://mail.python.org/pipermail/python-dev/2008-March/077926.html

Scribbles::

pull vs. push
user vs. system
web vs. filesystem
auto vs. manual

manage development sandboxes

optional interfaces...

    def get_data(pathname) -> string with file data.

    Return the data associated with 'pathname'. Raise IOError if
    the file wasn't found.");

    def is_package,
    "is_package(fullname) -> bool.

    Return True if the module specified by fullname is a package.
    Raise ZipImportError is the module couldn't be found.");

    def get_code,
    "get_code(fullname) -> code object.

    Return the code object for the specified module. Raise ZipImportError
    is the module couldn't be found.");

    def get_source,
    "get_source(fullname) -> source string.

    Return the source code for the specified module. Raise ZipImportError
    is the module couldn't be found, return None if the archive does
    contain the module, but has no source for it.");


Autoinstall can also be bootstraped with the nascent package loader
bootstrap module. For example::

    #  or via the bootstrap
    # loader.

    try:
        _version = "0.2"
        import autoinstall
        if autoinstall.__version__ != _version:
            raise ImportError("A different version than expected found.")
    except ImportError, e:
        # http://svn.python.org/projects/sandbox/trunk/bootstrap/bootstrap.py
        import bootstrap 
        pypi = "http://pypi.python.org"
        dir = "packages/source/a/autoinstall"
        url = "%s/%s/autoinstall-%s.tar.gz" % (pypi, dir, _version)
        bootstrap.main((url,))
        import autoinstall

References::

  http://0install.net/
  http://www.python.org/dev/peps/pep-0302/
  http://svn.python.org/projects/sandbox/trunk/import_in_py
  http://0install.net/injector-find.html
  http://roscidus.com/desktop/node/903

"""

# To allow use of the "with" keyword for Python 2.5 users.
from __future__ import with_statement

__version__ = "0.2"
__docformat__ = "restructuredtext en"

import os
import new
import sys
import urllib
import logging
import tempfile
import zipimport

_logger = logging.getLogger(__name__)


_importer = None

def _getImporter():
    global _importer
    if _importer is None:
        _importer = Importer()
        sys.meta_path.append(_importer)
    return _importer

def bind(package_name, url, zip_subpath=None):
    """bind a top level package name to a URL.

    The package name should be a package name and the url should be a
    url to something that can be imported using the zipimporter.

    Optional zip_subpath parameter allows searching for modules
    below the root level of the zip file.
    """
    _getImporter().bind(package_name, url, zip_subpath)


class Cache(object):

    def __init__(self, directory=None):
        if directory is None:
            # Default to putting the cache directory in the same directory
            # as this file.
            containing_directory = os.path.dirname(__file__)
            directory = os.path.join(containing_directory, "autoinstall.cache.d");

        self.directory = directory
        try:
            if not os.path.exists(self.directory):
                self._create_cache_directory()
        except Exception, err:
            _logger.exception(err)
            self.cache_directry = tempfile.mkdtemp()
        _logger.info("Using cache directory '%s'." % self.directory)
    
    def _create_cache_directory(self):
        _logger.debug("Creating cache directory '%s'." % self.directory)
        os.mkdir(self.directory)
        readme_path = os.path.join(self.directory, "README")
        with open(readme_path, "w") as f:
            f.write("This directory was auto-generated by '%s'.\n"
                    "It is safe to delete.\n" % __file__)

    def get(self, url):
        _logger.info("Getting '%s' from cache." % url)
        filename = url.rsplit("/")[-1]

        # so that source url is significant in determining cache hits
        d = os.path.join(self.directory, "%s" % hash(url))
        if not os.path.exists(d):
            os.mkdir(d)

        filename = os.path.join(d, filename) 

        if os.path.exists(filename):
            _logger.debug("... already cached in file '%s'." % filename)
        else:
            _logger.debug("... not in cache. Caching in '%s'." % filename)
            stream = file(filename, "wb")
            self.download(url, stream)
            stream.close()
        return filename

    def download(self, url, stream):
        _logger.info("Downloading: %s" % url)
        try:
            netstream = urllib.urlopen(url)
            code = 200
            if hasattr(netstream, "getcode"):
                code = netstream.getcode()
            if not 200 <= code < 300:
                raise ValueError("HTTP Error code %s" % code)
        except Exception, err:
            _logger.exception(err)

        BUFSIZE = 2**13  # 8KB
        size = 0
        while True:
            data = netstream.read(BUFSIZE)
            if not data:
                break
            stream.write(data)
            size += len(data)
        netstream.close()
        _logger.info("Downloaded %d bytes." % size)


class Importer(object):

    def __init__(self):
        self.packages = {}
        self.__cache = None

    def __get_store(self):
        return self.__store
    store = property(__get_store)

    def _get_cache(self):
        if self.__cache is None:
            self.__cache = Cache()
        return self.__cache
    def _set_cache(self, cache):
        self.__cache = cache
    cache = property(_get_cache, _set_cache)

    def find_module(self, fullname, path=None):
        """-> self or None.

        Search for a module specified by 'fullname'. 'fullname' must be
        the fully qualified (dotted) module name. It returns the
        zipimporter instance itself if the module was found, or None if
        it wasn't. The optional 'path' argument is ignored -- it's
        there for compatibility with the importer protocol.");
        """
        _logger.debug("find_module(%s, path=%s)" % (fullname, path))

        if fullname in self.packages:
            (url, zip_subpath) = self.packages[fullname]
            filename = self.cache.get(url)
            zip_path = "%s/%s" % (filename, zip_subpath) if zip_subpath else filename
            _logger.debug("fullname: %s url: %s path: %s zip_path: %s" % (fullname, url, path, zip_path))
            try:
                loader = zipimport.zipimporter(zip_path)
                _logger.debug("returning: %s" % loader)
            except Exception, e:
                _logger.exception(e)
                return None
            return loader
        return None

    def bind(self, package_name, url, zip_subpath):
        _logger.info("binding: %s -> %s subpath: %s" % (package_name, url, zip_subpath))
        self.packages[package_name] = (url, zip_subpath)


if __name__=="__main__":
    import logging
    #logging.basicConfig()
    logger = logging.getLogger()

    console = logging.StreamHandler()
    console.setLevel(logging.DEBUG)
    # set a format which is simpler for console use
    formatter = logging.Formatter('%(name)-12s: %(levelname)-8s %(message)s')
    # tell the handler to use this format
    console.setFormatter(formatter)
    # add the handler to the root logger
    logger.addHandler(console)
    logger.setLevel(logging.INFO)

    bind("pymarc", "http://pypi.python.org/packages/2.5/p/pymarc/pymarc-2.1-py2.5.egg")

    import pymarc

    print pymarc.__version__, pymarc.__file__

    assert pymarc.__version__=="2.1"

    d = _getImporter().cache.directory
    assert d in pymarc.__file__, "'%s' not found in pymarc.__file__ (%s)" % (d, pymarc.__file__)

    # Can now also bind to non top level packages. The packages
    # leading up to the package being bound will need to be defined
    # however.
    #
    # bind("rdf.plugins.stores.memory", 
    #      "http://pypi.python.org/packages/2.5/r/rdf.plugins.stores.memeory/rdf.plugins.stores.memory-0.9a-py2.5.egg")
    #
    # from rdf.plugins.stores.memory import Memory


