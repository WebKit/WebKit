.. _migrating-from-bazaar:

Migrating from Bazaar
#####################

.. |updatecommand| replace:: :command:`bzr up -r tag:bugzilla-$VERSION`
.. |diffcommand|   replace:: :command:`bzr diff > patch.diff`
.. |extstatusinfo| replace:: The command :command:`bzr status extensions/` should help you work out what you added, if anything.

.. include:: migrating-from-1.inc.rst

The old bzr.mozilla.org server has been decommissioned. This may not
be a problem but, in some cases, running some of the commands below will
make :command:`bzr` attempt to contact the server and time out. If and
only if that happens to you, you will need to switch to the new server,
as follows. Enter your Bugzilla directory and run:

:command:`bzr info`

and look at the Location: section of the output.
If it says "light checkout root" then run:

:command:`bzr -Ossl.cert_reqs=none switch https://bzr.bugzilla.org/bugzilla/$VERSION`

Alternatively, if it says "branch root" or "checkout root" then run:

:command:`bzr -Ossl.cert_reqs=none pull --remember https://bzr.bugzilla.org/bugzilla/$VERSION`

Replace $VERSION with the two-digit version number of your current
Bugzilla, e.g. "4.2" (see below for how to find that).

.. include:: migrating-from-2.inc.rst
