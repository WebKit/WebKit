.. _postgresql:

PostgreSQL
##########

Test which version of PostgreSQL you have installed with:

:command:`psql -V`

You need PostgreSQL version 8.03.0000 or higher.

If you install PostgreSQL manually rather than from a package, make sure the
server is started when the machine boots.

.. _posgresql-add-user:

Add a User
==========

You need to add a new user to PostgreSQL for the Bugzilla
application to use when accessing the database. The following instructions
assume the defaults in :file:`localconfig`; if you
changed those, you need to modify the commands appropriately.

On most systems, to create a user in PostgreSQL, login as the root user, and
then switch to being the postgres (Unix) user:

:command:`su - postgres`

As the postgres user, you then need to create a new user:

:command:`createuser -U postgres -dRSP bugs`

When asked for a password, provide one and write it down for later reference.

The created user will not be a superuser (-S) and will not be able to create
new users (-R). He will only have the ability to create databases (-d).

.. _postgresql-access:

Permit Access
=============

Edit the file :file:`pg_hba.conf` which is
usually located in :file:`/var/lib/pgsql/data/`. In this file,
you will need to add a new line to it as follows:

::

    host   all    bugs   127.0.0.1    255.255.255.255  md5

This means that for TCP/IP (host) connections, allow connections from
'127.0.0.1' to 'all' databases on this server from the 'bugs' user, and use
password authentication ('md5') for that user.

Now, you will need to stop and start PostgreSQL fully. (Do not use any
'restart' command, due to the possibility of a change to
:file:`postgresql.conf`.)
