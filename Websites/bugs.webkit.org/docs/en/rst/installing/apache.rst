.. This document is shared among all non-Windows OSes.

.. _apache:

Apache
######

You have two options for running Bugzilla under Apache - mod_cgi (the
default) and mod_perl. mod_perl is faster but takes more resources. You
should probably only consider mod_perl if your Bugzilla is going to be heavily
used.

These instructions require editing the Apache configuration file, which is:

* Fedora/Red Hat: :file:`/etc/httpd/conf/httpd.conf`
* Debian/Ubuntu: :file:`/etc/apache2/apache2.conf`
* Mac OS X: :file:`/etc/apache2/httpd.conf`

Alternatively, on Debian or Ubuntu, you can instead put the below code into a
separate file in the directory :file:`/etc/apache2/sites-enabled/`.

In these instructions, when asked to restart Apache, the command is:

:command:`sudo apachectl start`

(or run it as root if your OS installation does not use sudo).

Securing Apache
===============

When external systems interact with Bugzilla via webservices
(REST/XMLRPC/JSONRPC) they include the user's credentials as part of the URL
(in the "query string"). Therefore, to avoid storing passwords in clear text
on the server we recommend configuring Apache to not include the query string
in its log files.

#. Edit the Apache configuration file (see above).

#. Find the following line in the above mentioned file, which defines the
   logging format for ``vhost_combined``:

   .. code-block:: apache

      LogFormat "%v:%p %h %l %u %t \"%r\" %>s %O \"%{Referer}i\" \"%{User-Agent}i\"" vhost_combined

#. Replace ``%r`` with ``%m %U``.

#. Restart Apache.

.. _apache-mod_cgi:

Apache with mod_cgi
===================

To configure your Apache web server to work with Bugzilla while using
mod_cgi, do the following:

#. Edit the Apache configuration file (see above).

#. Create a ``<Directory>`` directive that applies to the location
   of your Bugzilla installation. In this example, Bugzilla has
   been installed at :file:`/var/www/html/bugzilla`. On Mac OS X, use
   :file:`/Library/WebServer/Documents/bugzilla`.

.. code-block:: apache

   <Directory /var/www/html/bugzilla>
     AddHandler cgi-script .cgi
     Options +ExecCGI +FollowSymLinks
     DirectoryIndex index.cgi index.html
     AllowOverride All
   </Directory>

These instructions allow Apache to run .cgi files found within the Bugzilla
directory; instructs the server to look for a file called :file:`index.cgi`
or, if not found, :file:`index.html` if someone only types the directory name
into the browser; and allows Bugzilla's :file:`.htaccess` files to override
some global permissions.

On some Linux distributions you will need to enable the Apache CGI
module. On Debian/Ubuntu, this is done with:

:command:`sudo a2enmod cgi`

If you find that the webserver is returning the Perl code as text rather
than executing it, then this is the problem.

.. _apache-mod_perl:

Apache with mod_perl
====================

Some configuration is required to make Bugzilla work with Apache
and mod_perl.

.. note:: It is not known whether anyone has even tried mod_perl on Mac OS X.

#. Edit the Apache configuration file (see above).

#. Add the following information, substituting where appropriate with your
   own local paths.

   .. code-block:: apache

       PerlSwitches -w -T
       PerlConfigRequire /var/www/html/bugzilla/mod_perl.pl

   .. note:: This should be used instead of the <Directory> block
      shown above. This should also be above any other ``mod_perl``
      directives within the :file:`httpd.conf` and the directives must be
      specified in the order above.

   .. warning:: You should also ensure that you have disabled ``KeepAlive``
      support in your Apache install when utilizing Bugzilla under mod_perl
      or you may suffer a
      `performance penalty <http://modperlbook.org/html/11-4-KeepAlive.html>`_.

On restarting Apache, Bugzilla should now be running within the
mod_perl environment.

Please bear the following points in mind when considering using Bugzilla
under mod_perl:

* mod_perl support in Bugzilla can take up a HUGE amount of RAM - easily
  30MB per httpd child. The more RAM you can get, the better. mod_perl is
  basically trading RAM for speed. At least 2GB total system RAM is
  recommended for running Bugzilla under mod_perl.
  
* Under mod_perl, you have to restart Apache if you make any manual change to
  any Bugzilla file. You can't just reload--you have to actually
  *restart* the server (as in make sure it stops and starts
  again). You *can* change :file:`localconfig` and the :file:`params` file
  manually, if you want, because those are re-read every time you load a page.

* You must run in Apache's Prefork MPM (this is the default). The Worker MPM
  may not work -- we haven't tested Bugzilla's mod_perl support under threads.
  (And, in fact, we're fairly sure it *won't* work.)

* Bugzilla generally expects to be the only mod_perl application running on
  your entire server. It may or may not work if there are other applications also
  running under mod_perl. It does try its best to play nice with other mod_perl
  applications, but it still may have conflicts.

* It is recommended that you have one Bugzilla instance running under mod_perl
  on your server. Bugzilla has not been tested with more than one instance running.
