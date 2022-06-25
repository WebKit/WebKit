.. _migrating-from-a-tarball:

Migrating from a Tarball
########################

.. |diffcommand|   replace:: :command:`diff -ru -x data -x lib -x docs -x .git -x CVS -x .cvsignore -x .bzr -x .bzrignore -x .bzrrev ../bugzilla-new . > ../patch.diff`

.. |extstatusinfo| replace:: Copy across any subdirectories which do not exist
                             in your new install.

The procedure to migrate to Git is as follows. The idea is to switch without
changing the version of Bugzilla you are using, to minimise the risk of
conflict or problems. Any upgrade can then happen as a separate step. 

.. include:: migrating-from-2.inc.rst
