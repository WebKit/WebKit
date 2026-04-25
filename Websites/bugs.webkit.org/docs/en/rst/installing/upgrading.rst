.. _upgrading:

Upgrading
#########

You can upgrade Bugzilla from any version to any later version in one go -
there is no need to pass through intermediate versions unless you are changing
the method by which you obtain the code along the way.

.. warning:: Upgrading is a one-way process. You cannot "downgrade" an
   upgraded Bugzilla. If you wish to revert to the old Bugzilla
   version for any reason, you will have to restore your system
   from a backup. Those with critical data or large installations may wish
   to test the upgrade on a development server first, using a copy of the
   production data and configuration.
 
Bugzilla uses the Git version control system to store its code. A modern
Bugzilla installation consists of a checkout of a stable version of the code
from our Git repository. This makes upgrading much easier. If this is
already true of your installation, see :ref:`upgrading-with-git`.

Before Git, we used to use Bazaar and, before that, CVS. If your installation
of Bugzilla consists of a checkout from one of those two systems, you need to
upgrade in three steps:

1. Upgrade to the latest point release of your current Bugzilla version.
2. Move to Git while staying on exactly the same release.
3. Upgrade to the latest Bugzilla using the instructions for :ref:`upgrading-with-git`.

See :ref:`migrating-from-bazaar` or :ref:`migrating-from-cvs` as appropriate.

Some Bugzillas were installed simply by downloading a copy of the code as
an archive file ("tarball"). However, recent tarballs have included source
code management system information, so you may be able to use the Git, Bzr
or CVS instructions.

If you aren't sure which of these categories you fall into, to find out which
version control system your copy of Bugzilla recognizes, look for the
following subdirectories in your root Bugzilla directory:

* :file:`.git`: you installed using Git - follow :ref:`upgrading-with-git`.
* :file:`.bzr`: you installed using Bazaar - follow :ref:`migrating-from-bazaar`.
* :file:`CVS`: you installed using CVS - follow :ref:`migrating-from-cvs`.
* None of the above: you installed using an old tarball - follow
  :ref:`migrating-from-a-tarball`.

It is also possible, particularly if your server machine does not have and
cannot be configured to have access to the public internet, to upgrade using
a tarball. See :ref:`upgrading-with-a-tarball`.

Whichever path you use, you may need help with
:ref:`upgrading-customizations`.

.. toctree::
   :maxdepth: 1

   upgrading-with-git
   migrating-from-bzr
   migrating-from-cvs
   migrating-from-a-tarball
   upgrading-with-a-tarball
   upgrading-customizations

Bugzilla can automatically notify administrators when new releases are
available if the :param:`upgrade_notification` parameter is set.
Administrators will see these notifications when they access the Bugzilla home
page. Bugzilla will check once per day for new releases. If you are behind a
proxy, you may have to set the :param:`proxy_url` parameter accordingly. If
the proxy requires authentication, use the
:paramval:`http://user:pass@proxy_url/` syntax.
