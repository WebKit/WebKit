.. _sqlite:

SQLite
######

.. warning:: Due to SQLite's `concurrency
   limitations <http://sqlite.org/faq.html#q5>`_ we recommend SQLite only for
   small and development Bugzilla installations.

Once you have SQLite installed, no additional configuration is required to
run Bugzilla.

The database will be stored in :file:`$BUGZILLA_HOME/data/db/$db_name`, where
``$db_name`` is the database name defined in :file:`localconfig`.
