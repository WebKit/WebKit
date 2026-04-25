.. _linux:

Linux
#####

Some Linux distributions include Bugzilla and its dependencies in their
package management systems. If you have root access, installing Bugzilla on
any Linux system could be as simple as finding the Bugzilla package in the
package management application and installing it. There may be a small bit
of additional configuration required.

If you are installing your machine from scratch, :ref:`quick-start` may be
the best instructions for you.

.. _linux-install-packages:

Install Packages
================

Use your distribution's package manager to install Perl, your preferred
database engine (MySQL or MariaDB if in doubt), and a webserver (Apache if in doubt).
Some distributions even have a Bugzilla package, although that will vary
in age.

The commands below will install those things and some of Bugzilla's other
prerequisites as well. If you find a package doesn't install or the name
is not found, just remove it from the list and reissue the command. If you
want to use a different database or webserver, substitute the package
names as appropriate.

Fedora, CentOS Stream and RHEL
------------------------------

The following command will install Fedora's packaged version of Bugzilla:

:command:`dnf install bugzilla httpd mariadb-server`

Then, you can skip to :ref:`configuring your database <linux-config-database>`.
It may be useful to know that Fedora stores the Bugzilla files in
:file:`/usr/share/bugzilla`, so that's where you'll run :file:`checksetup.pl`.

If you want to install a version of Bugzilla from the Bugzilla project
or have it on RHEL or CentOS, you will need to do the following instead:

On CentOS Stream and RHEl, add the Fedora EPEL repo, in the way described
in the `installation instructions <https://docs.fedoraproject.org/en-US/epel/>`_.

Run the following to install the base Bugzilla dependencies:

:command:`dnf install git httpd httpd-devel mariadb-devel gcc
mariadb-server mod_perl mod_perl-devel 'perl(autodie)' 'perl(CGI)'
'perl(Date::Format)' 'perl(DateTime)' 'perl(DateTime::TimeZone)'
'perl(DBI)' 'perl(DBD::mysql)' 'perl(Digest::SHA)' 'perl(Email::MIME)'
'perl(Email::Sender)' 'perl(fields)' 'perl(JSON::XS)'
'perl(List::MoreUtils)' 'perl(Math::Random::ISAAC)' 'perl(Memoize)'
'perl(Safe)' 'perl(Template)' 'perl(URI)'`

On Fedora, all the optional dependencies are available:

:command:`dnf install gd-devel graphviz patchutils
'perl(Apache2::SizeLimit)' 'perl(Authen::Radius)' 'perl(Authen::SASL)'
'perl(Cache::Memcached)' 'perl(Chart::Lines)' 'perl(Daemon::Generic)'
'perl(Email::Reply)' 'perl(Encode)' 'perl(Encode::Detect)'
'perl(File::Copy::Recursive)' 'perl(File::MimeInfo::Magic)'
'perl(File::Which)' 'perl(GD)' 'perl(GD::Graph)' 'perl(GD::Text)' 'perl(HTML::FormatText::WithLinks)' 'perl(HTML::Parser)'
'perl(HTML::Scrubber)' 'perl(IO::Scalar)' 'perl(JSON::RPC)'
'perl(LWP::UserAgent)' 'perl(MIME::Parser)' 'perl(mod_perl2)'
'perl(Net::LDAP)' 'perl(Net::SMTP::SSL)' 'perl(PatchReader)'
'perl(SOAP::Lite)' 'perl(Template::Plugin::GD::Image)'
'perl(Test::Taint)' 'perl(TheSchwartz)' 'perl(XMLRPC::Lite)'
'perl(XML::Twig)'`

On CentOS Stream and RHEL with EPEL, some modules are missing in the
repositories, so use the following instead:

:command:`dnf install gd-devel graphviz patchutils
'perl(Apache2::SizeLimit)' 'perl(Authen::Radius)' 'perl(Authen::SASL)'
'perl(Cache::Memcached)' 'perl(Encode)' 'perl(Encode::Detect)'
'perl(File::Copy::Recursive)' 'perl(File::MimeInfo::Magic)'
'perl(File::Which)' 'perl(GD)' 'perl(GD::Graph)' 'perl(GD::Text)'
'perl(HTML::Parser)' 'perl(HTML::Scrubber)' 'perl(IO::Scalar)'
'perl(JSON::RPC)' 'perl(LWP::UserAgent)' 'perl(MIME::Parser)'
'perl(mod_perl2)' 'perl(Net::LDAP)' 'perl(Net::SMTP::SSL)'
'perl(SOAP::Lite)' 'perl(Test::Taint)' 'perl(XMLRPC::Lite)'
'perl(XML::Twig)'`

and install the missing optional modules with:

:command:`cd /var/www/html/bugzilla/ && ./install-module.pl Chart::Lines
Daemon::Generic Email::Reply HTML::FormatText::WithLinks PatchReader
Template::Plugin::GD::Image TheSchwartz`

If you plan to use a database other than MariaDB, you will need to also install
the appropriate packages for that.

Ubuntu and Debian
-----------------

You can install required packages with:
:command:`apt install apache2 build-essential git
libcgi-pm-perl libdatetime-perl libdatetime-timezone-perl
libdbi-perl libdigest-sha-perl libemail-address-perl
libemail-mime-perl libemail-sender-perl libjson-xs-perl
liblist-moreutils-perl libmath-random-isaac-perl libtemplate-perl
libtimedate-perl liburi-perl libmariadb-dev-compat libdbd-mysql-perl
mariadb-server`

If you plan to use a database other than MariaDB, you will need to also install
the appropriate packages for that (in the command above, the packages required
for MariaDB are ``libdbd-mysql-perl``, ``libmariadb-dev-compat`` and ``mariadb-server``).

You can install optional packages with:
:command:`apt install graphviz libapache2-mod-perl2
libapache2-mod-perl2-dev libauthen-radius-perl libauthen-sasl-perl
libcache-memcached-perl libchart-perl libdaemon-generic-perl
libemail-reply-perl libencode-detect-perl libencode-perl
libfile-copy-recursive-perl libfile-mimeinfo-perl libfile-which-perl
libgd-dev libgd-graph-perl libgd-perl libgd-text-perl
libhtml-formattext-withlinks-perl libhtml-parser-perl
libhtml-scrubber-perl libio-stringy-perl libjson-rpc-perl
libmime-tools-perl libnet-ldap-perl libnet-smtp-ssl-perl
libsoap-lite-perl libtemplate-plugin-gd-perl libtest-taint-perl
libtheschwartz-perl libwww-perl libxmlrpc-lite-perl libxml-twig-perl`

There is no Ubuntu package for ``PatchReader`` and so you will have to install that module
outside the package manager if you want it.

Gentoo
------

:command:`emerge -av bugzilla`

will install Bugzilla and all its dependencies. If you don't have the vhosts
USE flag enabled, Bugzilla will end up in :file:`/var/www/localhost/bugzilla`.

Then, you can skip to :ref:`configuring your database
<linux-config-database>`.

.. _linux-install-perl:

openSUSE
--------

:command:`zypper in bugzilla`

has been available in the openSUSE Leap repositories since 15.2 and is
in the openSUSE Tumbleweed repositories, also comes with an optional
``bugzilla-apache`` package, that allows you to skip to
:ref:`configuring your database <linux-config-database>`

Perl
====

Test which version of Perl you have installed with:
::

    $ perl -v

Bugzilla requires at least Perl |min-perl-ver|.

.. _linux-install-bzfiles:

Bugzilla
========

The best way to get Bugzilla is to check it out from git:

:command:`git clone --branch release-X.X-stable https://github.com/bugzilla/bugzilla`

Run the above command in your home directory, replacing "X.X" with the 2-digit
version number of the stable release of Bugzilla that you want - e.g. "4.4".

If that's not possible, you can
`download a tarball of Bugzilla <http://www.bugzilla.org/download/>`_.

Place Bugzilla in a suitable directory, accessible by the default web server
user (probably ``apache`` or ``www-data``).
Good locations are either directly in the web server's document directory
(often :file:`/var/www/html`) or in :file:`/usr/local`, either with a
symbolic link to the web server's document directory or an alias in the web
server's configuration.

.. warning:: The default Bugzilla distribution is NOT designed to be placed
   in a :file:`cgi-bin` directory. This
   includes any directory which is configured using the
   ``ScriptAlias`` directive of Apache.

.. _linux-install-perl-modules:

Perl Modules
============

Bugzilla requires a number of Perl modules. You can install these globally
using your system's package manager, or install Bugzilla-only copies. At
times, Bugzilla may require a version of a Perl module newer than the one
your distribution packages, in which case you will need to install a
Bugzilla-only copy of the newer version.

At this point you probably need to become ``root``, e.g. by using
:command:`su`. You should remain as root until the end of the install. This
can be avoided in some circumstances if you are a member of your webserver's
group, but being root is easier and will always work.

To check whether you have all the required modules, run:

:command:`./checksetup.pl --check-modules`

You can run this command as many times as necessary.

If you have not already installed the necessary modules, and want to do it
system-wide, invoke your package manager appropriately at this point.
Alternatively, you can install all missing modules locally (i.e. just for
Bugzilla) like this:

:command:`./install-module.pl --all`

Or, you can pass an individual module name:

:command:`./install-module.pl <modulename>`

.. _linux-config-webserver:

Web Server
==========

Any web server that is capable of running CGI scripts can be made to work.
We have specific configuration instructions for the following:

* :ref:`apache`

.. _linux-config-database:

Database Engine
===============

Bugzilla supports MySQL (or MariaDB, its compatible counterpart), PostgreSQL,
Oracle and SQLite as database servers. You only require one of these systems
to make use of Bugzilla. MySQL or MariaDB are most commonly used. SQLite is
good for trial installations as it requires no setup. Configure your server
according to the instructions below:

* :ref:`mysql`
* :ref:`postgresql`
* :ref:`oracle`
* :ref:`sqlite`

.. |checksetupcommand| replace:: :command:`./checksetup.pl`
.. |testservercommand| replace:: :command:`./testserver.pl http://<your-bugzilla-server>/`

.. include:: installing-end.inc.rst

Next, do the :ref:`essential-post-install-config`.
