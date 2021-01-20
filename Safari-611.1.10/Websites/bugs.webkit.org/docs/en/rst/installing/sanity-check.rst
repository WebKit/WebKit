.. _sanity-check:

Sanity Check
############

Over time it is possible for the Bugzilla database to become corrupt
or to have anomalies. This could happen through manual database
administration outside of the Bugzilla user interface, or from some
other unexpected event. Bugzilla includes a "Sanity Check" that
can perform several basic database checks, and repair certain problems or
inconsistencies.

To run a Sanity Check, log in as an Administrator and click the
:guilabel:`Sanity Check` link in the admin page. Any problems that are found
will be displayed in red letters. If the script is capable of fixing a
problem, it will present a link to initiate the fix. If the script cannot
fix the problem it will require manual database administration or recovery.

Sanity Check can also be run from the command line via the perl
script :file:`sanitycheck.pl`. The script can also be run as
a :command:`cron` job. Results will be delivered by email to an address
specified on the command line.

Sanity Check should be run on a regular basis as a matter of
best practice.

