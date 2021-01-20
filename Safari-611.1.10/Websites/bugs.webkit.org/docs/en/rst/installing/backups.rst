.. _backups:

Backups
#######

Database
========

Here are some sample commands you could use to backup
your database, depending on what database system you're
using. You may have to modify these commands for your
particular setup. Replace the $VARIABLEs with appropriate values for your
setup.

MySQL
-----

:command:`mysqldump --max-allowed-packet=32M -u $USERNAME -p $DATABASENAME > backup.sql`

The value for :command:`max-allowed-packet` should be the value you've set in
your :ref:`MySQL configuration file <mysql>`, and should be larger than the
largest attachment in your database. See the
`mysqldump documentation <http://dev.mysql.com/doc/mysql/en/mysqldump.html>`_
for more information on :file:`mysqldump`.

PostgreSQL
----------

:command:`pg_dump --no-privileges --no-owner -h localhost -U $USERNAME > bugs.sql`

Bugzilla
========

The Bugzilla directory contains some data files and configuration files which
you would want to retain. A simple recursive copy will do the job here.

:command:`cp -rp $BUGZILLA_HOME /var/backups/bugzilla`

