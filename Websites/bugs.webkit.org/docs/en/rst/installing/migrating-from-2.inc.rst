.. This file is included in multiple places, so can't have labels as they
   appear as duplicates.

Download Code from Git
======================

First, you need to find what version of Bugzilla you are using. It should be
in the top right corner of the front page but, if not, open the file
:file:`Bugzilla/Constants.pm` in your Bugzilla directory and search for
``BUGZILLA_VERSION``.

Then, you need to download an additional copy of your *current* version of
Bugzilla from the git repository, and place it in a separate directory
alongside your existing Bugzilla installation (which we will assume is in a
directory called :file:`bugzilla`).

To do this, you will need a copy of the :command:`git` program. All Linux
distributions have it; search your package manager for "git". On Windows or
Mac OS X, you can
`download the official build <http://www.git-scm.com/downloads>`_.

Once git is installed, run these commands to pull a copy of Bugzilla:

:command:`git clone https://github.com/bugzilla/bugzilla bugzilla-new`

:command:`cd bugzilla-new`

:command:`git checkout release-$VERSION`

Replace $VERSION with the three-digit version number of your current Bugzilla,
e.g. "4.2.2". (If the the final digit would have been a 0, omit it - so use
"4.4" for the first release in the 4.4 series.)

You will get a message about a 'detached HEAD'. Don't worry; your head is
still firmly attached to your shoulders.

Save Any Local Customizations
=============================

Go into your original Bugzilla directory and run this command:

|diffcommand|

If you have made customizations to your Bugzilla, and you made them by
changing the Bugzilla code itself (rather than using the Extension system),
then :file:`patch.diff` will have significant content. You will want to keep a copy
of those changes by keeping a copy of this file and any files referenced in it
by "Only in" lines. If the file has zero size or only insignificant content,
you haven't made any local customizations of this sort.

Shut Down Bugzilla
==================

At this point, you should shut down Bugzilla to make sure nothing changes
while you make the switch. Go into the administrative interface and put an
appropriate message into the :param:`shutdownhtml` parameter, which is in the
"General" section of the administration parameters. As the name implies, HTML
is allowed.

This would be a good time to make :ref:`backups`. We shouldn't be affecting
the database, but you can't be too careful.

Copy Across Data and Modules
============================

Copy the contents of the following directories from your current installation
of Bugzilla into the corresponding directory in :file:`bugzilla-new/`:

.. code-block:: none

  lib/
  data/
  template/en/custom (may or may not exist)

You also need to copy any extensions you have written or installed, which are
in the :file:`extensions/` directory. |extstatusinfo|

Lastly, copy the following file from your current installation of Bugzilla
into the corresponding place in :file:`bugzilla-new/`:

.. code-block:: none

  localconfig

This file contains your database password and access details. Because your
two versions of Bugzilla are the same, this should all work fine.

Reapply Local Customizations
============================

If your :file:`patch.diff` file was zero sized, you can
jump to the next step. Otherwise, you have to apply the patch to your new
installation. If you are on Windows and you don’t have the :command:`patch`
program, you can download it from
`GNUWin <http://gnuwin32.sourceforge.net/packages/patch.htm>`_. Once
downloaded, you must copy patch.exe into the Windows directory. 

Copy :file:`patch.diff` into the :file:`bugzilla-new` directory and then do:

:command:`patch -p0 --dry-run < patch.diff`

The patch should apply cleanly because you have exactly the same version of
Bugzilla in both directories. If it does, remove the :command:`--dry-run` and
rerun the command to apply it for real. If it does not apply cleanly, it is
likely that you have managed to get a Bugzilla version mismatch between the
two directories.

Swap The New Version In
=======================

Now we swap the directories over, and run checksetup.pl to confirm that all
is well. From the directory containing the :file:`bugzilla` and
:file:`bugzilla-new` directories, run:

:command:`mv bugzilla bugzilla-old`

:command:`mv bugzilla-new bugzilla`

:command:`cd bugzilla`

:command:`./checksetup.pl`

Running :file:`checksetup.pl` should not result in any changes to your database at
the end of the run. If it does, then it's most likely that the two versions
of Bugzilla you have are not, in fact, the same.

Re-enable Bugzilla
==================

Go into the administrative interface and clear the contents of the
:param:`shutdownhtml` parameter.

Test Bugzilla
=============

Use your Bugzilla for several days to check that the switch has had no
detrimental effects. Then, if necessary, follow the instructions in
:ref:`upgrading-with-git` to upgrade to the latest version of Bugzilla.

Rolling Back
============

If something goes wrong at any stage of the switching process (e.g. your
patch doesn't apply, or checksetup doesn't complete), you can always just
switch the directories back (if you've got that far) and re-enable Bugzilla
(if you disabled it) and then seek help. Even if you have re-enabled Bugzilla,
and find a problem a little while down the road, you are still using the same
version so there would be few side effects to switching the directories back
a day or three later.
