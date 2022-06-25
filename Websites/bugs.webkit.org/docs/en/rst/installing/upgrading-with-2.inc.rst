Upgrading the Database
======================

Run :file:`checksetup.pl`. This will do everything required to convert
your existing database and settings to the new version.

:command:`cd $BUGZILLA_HOME`

:command:`./checksetup.pl`

   .. warning:: For some upgrades, running :file:`checksetup.pl` on a large
      installation (75,000 or more bugs) can take a long time,
      possibly several hours, if e.g. indexes need to be rebuilt. If this
      length of downtime would be a problem for you, you can determine
      timings for your particular situation by doing a test upgrade on a
      development server with the production data.

:file:`checksetup.pl` may also tell you that you need some additional
Perl modules, or newer versions of the ones you have. You will need to
install these, either system-wide or using the :file:`install-module.pl`
script that :file:`checksetup.pl` recommends.

Finishing The Upgrade
=====================

#. Reactivate Bugzilla by clear the text that you put into the
   :param:`shutdownhtml` parameter.

#. Run another :ref:`sanity-check` on your
   upgraded Bugzilla. It is recommended that you fix any problems
   you see immediately. Failure to do this may mean that Bugzilla
   may not work entirely correctly. 
