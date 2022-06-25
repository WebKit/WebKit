.. _apache-windows:

Apache on Windows
#################

Bugzilla supports all versions of Apache 2.2.x and 2.4.x.

Installing
==========

Download the Apache HTTP Server as a :file:`.zip` archive either from the
`Apache Lounge website <http://www.apachelounge.com/download>`_ or from the
`Apache Haus website <http://www.apachehaus.com/cgi-bin/download.plx>`_.

Unzip the archive into :file:`C:\\Apache24`. If you move it elsewhere, then
you must edit several variables in :file:`httpd.conf`, including ``ServerRoot``
and ``DocumentRoot``.

You must now edit the Apache configuration file :file:`C:\\Apache24\\conf\\httpd.conf`
and do the following steps:

#. Uncomment ``LoadModule cgi_module modules/mod_cgi.so`` at the beginning of the
   file to enable CGI support.
#. Uncomment ``AddHandler cgi-script .cgi`` to register :file:`.cgi` files
   as CGI scripts. For this handler to work, you must create a key in the
   Windows registry named ``HKEY_CLASSES_ROOT\.cgi\Shell\ExecCGI\Command`` with
   the default value pointing to the full path of :file:`perl.exe` with a ``-T``
   parameter. For example :file:`C:\\Perl\\bin\\perl.exe -T` if you use ActivePerl,
   or :file:`C:\\Strawberry\\perl\\bin\\perl.exe -T` if you use Strawberry Perl.
#. Add an Alias and a Directory for Bugzilla:

.. code-block:: apache

    Alias "/bugzilla/" "C:/bugzilla/"
    <Directory "C:/bugzilla">
        ScriptInterpreterSource Registry-Strict
        Options +ExecCGI +FollowSymLinks
        DirectoryIndex index.cgi index.html
        AllowOverride All
        Require all granted
    </Directory>

.. warning:: The above block takes a simple approach to access control and is
             correct for Apache 2.4. For Apache 2.2, replace ``Require all granted``
             with ``Allow from all``. If you have other access control
             requirements, you may need to make further modifications.

You now save your changes and start Apache as a service. From the Windows
command line (:file:`cmd.exe`):

:command:`C:\\Apache24\\bin>httpd.exe -k install`

That's it! Bugzilla is now accessible from http://localhost/bugzilla.

Apache Account Permissions
==========================

By default Apache installs itself to run as the SYSTEM account. For security
reasons it's better the reconfigure the service to run as an Apache user.
Create a new Windows user that is a member of **no** groups, and reconfigure
the Apache2 service to run as that account.

Whichever account you are running Apache as, SYSTEM or otherwise, needs write
and modify access to the following directories and all their subdirectories.
Depending on your version of Windows, this access may already be granted.

* :file:`C:\\Bugzilla\\data`
* :file:`C:\\Apache24\\logs`
* :file:`C:\\Windows\\Temp`

Note that :file:`C:\\Bugzilla\\data` is created the first time you run
:file:`checksetup.pl`.

Logging
=======

Unless you want to keep statistics on how many hits your Bugzilla install is
getting, it's a good idea to disable logging by commenting out the
``CustomLog`` directive in the Apache config file.

If you don't disable logging, you should at least disable logging of "query
strings". When external systems interact with Bugzilla via webservices
(REST/XMLRPC/JSONRPC) they include the user's credentials as part of the URL
(in the query string). Therefore, to avoid storing passwords in clear text
on the server we recommend configuring Apache to not include the query string
in its log files.

#. Find the following line in the Apache config file, which defines the
   logging format for ``vhost_combined``:

   .. code-block:: apache

      LogFormat "%v:%p %h %l %u %t \"%r\" %>s %O \"%{Referer}i\" \"%{User-Agent}i\"" vhost_combined

#. Replace ``%r`` with ``%m %U``.

(If you have configured Apache differently, a different log line might apply.
Adjust these instructions accordingly.)

Using Apache with SSL
=====================

If you want to enable SSL with Apache, i.e. access Bugzilla from
https://localhost/bugzilla, you need to do some extra steps:

#. Edit :file:`C:\\Apache24\\conf\\httpd.conf` and uncomment these lines:

   * ``LoadModule ssl_module modules/mod_ssl.so``
   * ``LoadModule socache_shmcb_module modules/mod_socache_shmcb.so``
   * ``Include conf/extra/httpd-ssl.conf``

#. Create your :file:`.key` and :file:`.crt` files using :file:`openssl.exe`
   provided with Apache:

   :command:`C:\\Apache24\\bin>openssl.exe req -x509 -nodes -days 730 -newkey rsa:2048 -keyout server.key -out server.crt`

   :file:`openssl.exe` will ask you a few questions about your location and
   your company name to populate fields of the certificate.

#. Once the key and the certificate for your server are generated, move them
   into :file:`C:\\Apache24\\conf` so that their location matches the
   ``SSLCertificateFile`` and ``SSLCertificateKeyFile`` variables defined in
   :file:`C:\\Apache24\\conf\\extra\\httpd-ssl.conf` (which you don't need to
   edit).

.. note:: This process leads to a self-signed certificate which will generate
         browser warnings on first visit. If your Bugzilla has a public DNS
         name, you can get a cert from a CA which will not have this problem.

Restart Apache
==============

Finally, restart Apache to pick up the changes, either from the Services
console or from the command line:

:command:`C:\\Apache24\\bin>httpd.exe -k restart`
