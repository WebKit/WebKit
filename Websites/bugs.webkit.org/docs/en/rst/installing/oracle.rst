.. _oracle:

Oracle
######

.. warning:: Bugzilla supports Oracle, but none of the current developers run
   it. Your mileage may vary.

You need Oracle version 10.02.0 or later.

.. _oracle-tablespace:

Create a New Tablespace
=======================

You can use the existing tablespace or create a new one for Bugzilla.
To create a new tablespace, run the following command:

::

    CREATE TABLESPACE bugs
    DATAFILE '*$path_to_datafile*' SIZE 500M
    AUTOEXTEND ON NEXT 30M MAXSIZE UNLIMITED

Here, the name of the tablespace is 'bugs', but you can
choose another name. *$path_to_datafile* is
the path to the file containing your database, for instance
:file:`/u01/oradata/bugzilla.dbf`.
The initial size of the database file is set in this example to 500 Mb,
with an increment of 30 Mb everytime we reach the size limit of the file.

.. _oracle-add-user:

Add a User to Oracle
====================

The user name and password must match what you set in :file:`localconfig`
(``$db_user`` and ``$db_pass``, respectively). Here, we assume that
the user name is 'bugs' and the tablespace name is the same
as above.

::

    CREATE USER bugs
    IDENTIFIED BY "$db_pass"
    DEFAULT TABLESPACE bugs
    TEMPORARY TABLESPACE TEMP
    PROFILE DEFAULT;
    -- GRANT/REVOKE ROLE PRIVILEGES
    GRANT CONNECT TO bugs;
    GRANT RESOURCE TO bugs;
    -- GRANT/REVOKE SYSTEM PRIVILEGES
    GRANT UNLIMITED TABLESPACE TO bugs;
    GRANT EXECUTE ON CTXSYS.CTX_DDL TO bugs;

.. _oracle_webserver:

Configure the Web Server
========================

If you use Apache, append these lines to :file:`httpd.conf`
to set ORACLE_HOME and LD_LIBRARY_PATH. For instance:

.. code-block:: apache

    SetEnv ORACLE_HOME /u01/app/oracle/product/10.2.0/
    SetEnv LD_LIBRARY_PATH /u01/app/oracle/product/10.2.0/lib/

When this is done, restart your web server.
