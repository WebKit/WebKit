.. _moving:

Moving Bugzilla Between Machines
################################

Sometimes it's necessary to take a working installation of Bugzilla and move
it to new hardware. This page explains how to do that, assuming that you
have Bugzilla's webserver and database on the same machine, and you are moving
both of them.

You are advised to install the same version of Bugzilla on the new
machine as the old machine - any :ref:`upgrade <upgrading>` you also need to
do can then be done as a separate step. But if you do install a newer version,
things should still work.

1. Shut down your Bugzilla by loading the front page, going to
   :guilabel:`Administration` | :guilabel:`Parameters` | :guilabel:`General`
   and putting some explanatory text into the :param:`shutdownhtml` parameter.

2. Make a :ref:`backup <backups>` of the bugs database.

3. On your new machine, install Bugzilla using the instructions at
   :ref:`installing`. Look at the old machine if you need to know what values
   you used for configuring e.g. MySQL.

4. Copy the :file:`data` directory and the :file:`localconfig` file from the
   old Bugzilla installation to the new one.

5. If anything about your database configuration changed (location of the
   server, username, password, etc.) as part of the move, update the
   appropriate variables in :file:`localconfig`.

6. If the new URL to your new Bugzilla installation is different from the old
   one, update the :param:`urlbase` parameter in :file:`data/params.json`
   using a text editor.

7. Copy the database backup file from your old server to the new one.

8. Create an empty ``bugs`` database on the new server. For MySQL, that would
   look like this:

   :command:`mysql -u root -p -e "CREATE DATABASE bugs DEFAULT CHARACTER SET utf8;"`

9. Import your backup file into your new ``bugs`` database. Again, for MySQL:

   :command:`mysql -u root -p bugs < $BACKUP_FILE_NAME`

   If you get an error about "packet too large" or "MySQL server has gone
   away", you need to adjust the ``max_allowed_packet`` setting in
   your :file:`my.cnf` file (usually :file:`/etc/my.cnf`) file to match or
   exceed the value configured in the same file in your old version of MySQL.

   If there are *any* errors during this step, you have to work out what
   went wrong, and then drop the database, create it again using the step
   above, and run the import again.

10. Run :file:`checksetup.pl` to make sure all is OK.
    (Unless you are using a newer version of Bugzilla on your new server, this
    should not make any changes.)

    :command:`./checksetup.pl`

11. Activate your new Bugzilla by loading the front page on the new server,
    going to :guilabel:`Administration` | :guilabel:`Parameters` |
    :guilabel:`General` and removing the text from the :param:`shutdownhtml`
    parameter.
