# Required for Python to search this directory for module files

import os

# Add containing "webkitpy" package directory to search path.
__path__.append(os.path.join(__path__[0], ".."))
