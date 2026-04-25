.. _migrating-from-other-btses:

Migrating From Other Bug-Tracking Systems
#########################################

Bugzilla has a framework you can use for migrating from other bug-tracking
systems - :api:`Bugzilla::Migrate <Bugzilla/Migrate.html>`.
It provides the infrastructure you will need,
but requires a module to be written to define the specifics of the system you
are coming from. One exists for
`Gnats <https://www.gnu.org/software/gnats/>`_. If you write one for a
popular system, please share your code with us.

Alternatively, Bugzilla comes with a script, :file:`importxml.pl`, which
imports bugs in Bugzilla's XML format. You can see examples of this format
by clicking the :guilabel:`XML` link at the bottom of a bug in a running
Bugzilla. You would need to read the script to see how it handles errors,
default values, creating non-existing values and so on.

Bugzilla::Migrate is preferred if possible.


