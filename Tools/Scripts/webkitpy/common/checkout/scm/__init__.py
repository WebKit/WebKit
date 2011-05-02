# Required for Python to search this directory for module files

# We only export public API here.
from .commitmessage import CommitMessage
from .detection import find_checkout_root, default_scm, detect_scm_system
from .git import Git, AmbiguousCommitError
from .scm import SCM, AuthenticationError, CheckoutNeedsUpdate
from .svn import SVN
