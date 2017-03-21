Before You Upgrade
==================

Before you start your upgrade, there are a few important
steps to take:

#. Read the
   `Release Notes <http://www.bugzilla.org/releases/>`_ of the version you're
   upgrading to and all intermediate versions, particularly the "Notes for
   Upgraders" sections, if present. They may make you aware of additional
   considerations.

#. Run the :ref:`sanity-check` on your installation. Attempt to fix all
   warnings that the page produces before you start, or it's
   possible that you may experience problems during your upgrade.

#. Work out how to :ref:`back up <backups>` your Bugzilla entirely, and
   how to restore from a backup if need be.

Customized Bugzilla?
--------------------

If you have modified the code or templates of your Bugzilla,
then upgrading requires a bit more thought and effort than the simple process
below. See :ref:`template-method` for a discussion of the various methods of
code customization that may have been used.

The larger the jump you are trying to make, the more difficult it
is going to be to upgrade if you have made local code customizations.
Upgrading from 4.2 to 4.2.1 should be fairly painless even if
you are heavily customized, but going from 2.18 to 4.2 is going
to mean a fair bit of work re-writing your local changes to use
the new files, logic, templates, etc. If you have done no local
changes at all, however, then upgrading should be approximately
the same amount of work regardless of how long it has been since
your version was released.

If you have made customizations, you should do the upgrade on a test system
with the same configuration and make sure all your customizations still work.
If not, port and test them so you have them ready to reapply once you do
the upgrade for real.
