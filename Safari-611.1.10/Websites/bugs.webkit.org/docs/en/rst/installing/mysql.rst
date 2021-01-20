.. _mysql:

MySQL
#####

You need MySQL version 5.0.15 or higher.

It's possible to test which version of MySQL you have installed with:

:command:`mysql -V`

Installing
==========

Windows
-------

Download the MySQL 32-bit or 64-bit MSI installer from the
`MySQL website <http://www.mysql.com/downloads/mysql/>`_ (~28 MB).

MySQL has a standard Windows installer. It's ok to select a Typical MySQL
install (the default). The rest of this documentation assumes assume you
have installed MySQL into :file:`C:\\mysql`. Adjust paths appropriately if not.

Linux/Mac OS X
--------------

The package install instructions given previously should have installed MySQL
on your machine, if it didn't come with it already. Run:

:command:`mysql_secure_installation`

and follow its advice.

If you did install MySQL manually rather than from a package, make sure the
server is started when the machine boots.

.. _mysql-add-user:

Add a User
==========

You need to add a new MySQL user for Bugzilla to use. Run the :file:`mysql`
command-line client and enter:

::

    GRANT SELECT, INSERT,
    UPDATE, DELETE, INDEX, ALTER, CREATE, LOCK TABLES,
    CREATE TEMPORARY TABLES, DROP, REFERENCES ON bugs.*
    TO bugs@localhost IDENTIFIED BY '$DB_PASS';

    FLUSH PRIVILEGES;

You need to replace ``$DB_PASS`` with a strong password you have chosen.
Write that password down somewhere.

The above command permits an account called ``bugs``
to connect from the local machine, ``localhost``. Modify the command to
reflect your setup if you will be connecting from another
machine or as a different user.

Change Configuration
====================

To change MySQL's configuration, you need to edit your MySQL
configuration file, which is:

* Red Hat/Fedora: :file:`/etc/my.cnf`
* Debian/Ubuntu: :file:`/etc/mysql/my.cnf`
* Windows: :file:`C:\\mysql\\bin\\my.ini`
* Mac OS X: :file:`/etc/my/cnf`

.. _mysql-max-allowed-packet:

Allow Large Attachments and Many Comments
-----------------------------------------

By default on some systems, MySQL will only allow you to insert things
into the database that are smaller than 1MB.

Bugzilla attachments
may be larger than this. Also, Bugzilla combines all comments
on a single bug into one field for full-text searching, and the
combination of all comments on a single bug could in some cases
be larger than 1MB.

We recommend that you allow at least 16MB packets by
adding or altering the ``max_allowed_packet`` parameter in your MySQL
configuration in the ``[mysqld]`` section, so that the number is at least
16M, like this (note that it's ``M``, not ``MB``):

::

    [mysqld]
    # Allow packets up to 16M
    max_allowed_packet=16M

.. _mysql-small-words:

Allow Small Words in Full-Text Indexes
--------------------------------------

By default, words must be at least four characters in length
in order to be indexed by MySQL's full-text indexes. This causes
a lot of Bugzilla-specific words to be missed, including "cc",
"ftp" and "uri".

MySQL can be configured to index those words by setting the
``ft_min_word_len`` param to the minimum size of the words to index.

::

    [mysqld]
    # Allow small words in full-text indexes
    ft_min_word_len=2

.. _mysql-attach-table-size:

Permit Attachments Table to Grow Beyond 4GB
===========================================

This is optional configuration for Bugzillas which are expected to become
very large, and needs to be done after Bugzilla is fully installed.

By default, MySQL will limit the size of a table to 4GB.
This limit is present even if the underlying filesystem
has no such limit.  To set a higher limit, run the :file:`mysql`
command-line client and enter the following, replacing ``$bugs_db``
with your Bugzilla database name (which is ``bugs`` by default):

.. code-block:: sql

    USE $bugs_db;
    
    ALTER TABLE attachments AVG_ROW_LENGTH=1000000, MAX_ROWS=20000;

The above command will change the limit to 20GB. MySQL will have
to make a temporary copy of your entire table to do this, so ideally
you should do this when your attachments table is still small.

.. note:: If you have set the setting in Bugzilla which allows large
   attachments to be stored on disk, the above change does not affect that.
