.. _upgrading-with-git:

Upgrading with Git
##################

Upgrading to new Bugzilla releases is very simple, and you can upgrade
from any version to any later version in one go - there is no need for
intermediate steps. There is a script named :file:`checksetup.pl` included
with Bugzilla that will automatically do all of the database migration
for you.

Bugzilla is now hosted on Github, but we used to be hosted on git.mozilla.org.
If you got the code from git.mozilla.org, you need to point your
checkout at Github instead. To find out, run:

:command:`git remote -v`

If you see "git.mozilla.org" anywhere in the output, then run:

:command:`git remote set-url origin https://github.com/bugzilla/bugzilla`

This change will only ever need to be done once.

.. include:: upgrading-with-1.inc.rst

You can see if you have local code customizations using:

:command:`git diff`

If that comes up empty, then run:

:command:`git log | head`

and see if the last commit looks like one made by the Bugzilla team, or
by you. If it looks like it was made by us, then you have made no local
code customizations.

.. _start-upgrade-git:

Starting the Upgrade
====================

When you are ready to go:

#. Shut down your Bugzilla installation by putting some explanatory text
   in the :param:`shutdownhtml` parameter.

#. Make all necessary :ref:`backups <backups>`.
   *THIS IS VERY IMPORTANT*. If anything goes wrong during the upgrade,
   having a backup allows you to roll back to a known good state.

.. _upgrade-files-git:

Getting The New Bugzilla
========================

In the commands below, ``$BUGZILLA_HOME`` represents the directory
in which Bugzilla is installed. Assuming you followed the installation
instructions and your Bugzilla is a checkout of a stable branch,
you can get the latest point release of your current version by simply doing:

:command:`cd $BUGZILLA_HOME`

:command:`git pull`

If you want to upgrade to a newer release of Bugzilla, then you will
additionally need to do:

:command:`git checkout release-X.X-stable`

where "X.X" is the 2-digit version number of the stable version you want to
upgrade to (e.g. "4.4").

.. note:: Do not attempt to downgrade Bugzilla this way - it won't work.

If you have local code customizations, git will attempt to merge them. If
it fails, then you should implement the plan you came up with when you
detected these customizations in the step above, before you started the
upgrade.

.. include:: upgrading-with-2.inc.rst
