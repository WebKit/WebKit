.. _upgrading-with-a-tarball:

Upgrading with a Tarball
########################

If you are unable (or unwilling) to use Git, another option is to obtain a
tarball of the latest version from our website and upgrade your Bugzilla
installation using that.

Without a source code management system to help you, the process may be
trickier.

.. include:: upgrading-with-1.inc.rst

As you are using a tarball and not an SCM, it's not at all easy to see if
you've made local code customizations. You may have to use institutional
knowledge, or download a fresh copy of your *current* version of Bugzilla
and compare the two directories. If you find that you have, you'll need
to turn them into a patch file, perhaps by diffing the two directories,
and then reapply that patch file later. If you are customizing Bugzilla
locally, please consider
:ref:`rebasing your install on top of git <migrating-from-a-tarball>`.

.. _upgrade-files-tarball:

Getting The New Bugzilla
========================

Download a copy of the latest version of Bugzilla from the
`Download Page <http://www.bugzilla.org/download/>`_ into a separate
directory (which we will call :file:`bugzilla-new`) alongside your existing
Bugzilla installation (which we will assume is in a directory called
:file:`bugzilla`).

Copy Across Data and Modules
============================

Copy the contents of the following directories from your current installation
of Bugzilla into the corresponding directory in :file:`bugzilla-new/`:

.. code-block:: none

  lib/
  data/
  template/en/custom (may or may not exist)

You also need to copy any extensions you have written or installed, which are
in the :file:`extensions/` directory. Bugzilla ships with some extensions,
so again if you want to know if any of the installed extensions are yours,
you may have to compare with a clean copy of your current version. You can
disregard any which have a :file:`disabled` file - those are not enabled.

Lastly, copy the following file from your current installation of Bugzilla
into the corresponding place in :file:`bugzilla-new/`:

.. code-block:: none

  localconfig

This file contains your database password and access details.

Swap The New Version In
=======================

Now we swap the directories over. From the directory containing the
:file:`bugzilla` and :file:`bugzilla-new` directories, run:

:command:`mv bugzilla bugzilla-old`

:command:`mv bugzilla-new bugzilla`

:command:`cd bugzilla`

.. include:: upgrading-with-2.inc.rst
