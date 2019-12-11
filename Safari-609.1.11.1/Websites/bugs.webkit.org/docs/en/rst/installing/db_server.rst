.. _db_server:

Database Server
###############

Bugzilla requires a database to store its data. We recommend either MySQL or
PostgreSQL for production installations. Oracle 10 should work fine, but very
little or no testing has been done with Oracle 11 and 12. SQLite is easy to
configure but, due to its limitations, it should only be used for testing
purposes and very small installations.

.. toctree::
   :maxdepth: 1

   mysql
   postgresql
   oracle
   sqlite
