localconfig
===========

You should now change into the Bugzilla directory and run
:file:`checksetup.pl`, without any parameters:

|checksetupcommand|

:file:`checksetup.pl` will write out a file called :file:`localconfig`.
This file contains the default settings for a number of
Bugzilla parameters, the most important of which are the group your web
server runs as, and information on how to connect to your database.

Load this file in your editor. You will need to check/change ``$db_driver``
and ``$db_pass``, which are respectively the type of the database you are
using and the password for the ``bugs`` database user you have created.
``$db_driver`` can be either ``mysql``, ``Pg`` (PostgreSQL), ``Oracle`` or
``Sqlite``. All values are case sensitive.

Set the value of ``$webservergroup`` to the group your web server runs as.

* Fedora/Red Hat: ``apache``
* Debian/Ubuntu: ``www-data``
* Mac OS X: ``_www``
* Windows: ignore this setting; it does nothing

The other options in the :file:`localconfig` file are documented by their
accompanying comments. If you have a non-standard database setup, you may
need to change one or more of the other ``$db_*`` parameters.

.. note:: If you are using Oracle, ``$db_name`` should be set to
   the SID name of your database (e.g. ``XE`` if you are using Oracle XE).

checksetup.pl
=============

Next, run :file:`checksetup.pl` an additional time:

|checksetupcommand|

It reconfirms that all the modules are present, and notices the altered
localconfig file, which it assumes you have edited to your
satisfaction. It compiles the UI templates,
connects to the database using the ``bugs``
user you created and the password you defined, and creates the
``bugs`` database and the tables therein.

After that, it asks for details of an administrator account. Bugzilla
can have multiple administrators - you can create more later - but
it needs one to start off with.
Enter the email address of an administrator, his or her full name,
and a suitable Bugzilla password.

:file:`checksetup.pl` will then finish. You may rerun
:file:`checksetup.pl` at any time if you wish.

Success
=======

Your Bugzilla should now be working. Check by running:

|testservercommand|

If that passes, access ``http://<your-bugzilla-server>/`` in your browser -
you should see the Bugzilla front page. Of course, if you installed Bugzilla
in a subdirectory, make sure that's in the URL.
