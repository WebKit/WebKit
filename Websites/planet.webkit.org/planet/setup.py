#!/usr/bin/env python
"""The Planet Feed Aggregator"""

import os
from distutils.core import setup

from planet import __version__ as VERSION
from planet import __license__ as LICENSE

if 'PLANET_VERSION' in os.environ.keys():
    VERSION = os.environ['PLANET_VERSION']

setup(name="planet",
      version=VERSION,
      description="The Planet Feed Aggregator",
      author="Planet Developers",
      author_email="devel@lists.planetplanet.org",
      url="http://www.planetplanet.org/",
      license=LICENSE,
      packages=["planet", "planet.compat_logging", "planet.tests"],
      scripts=["planet.py", "planet-cache.py", "runtests.py"],
      )
