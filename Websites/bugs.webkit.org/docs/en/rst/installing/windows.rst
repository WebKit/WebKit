.. _windows:

Windows
#######

Making Bugzilla work on Windows is not more difficult than making it work on
Linux. However, fewer developers use Windows to test Bugzilla and so we would
still recommend using Linux for large sites to get better support.

.. windows-install-perl:

Perl
====

You have two main choices to install Perl on Windows: ActivePerl and Strawberry
Perl.

The ActivePerl Windows Installer can be downloaded from the
`ActiveState website <http://www.activestate.com/activeperl/downloads>`_.
Perl will be installed by default into :file:`C:\\Perl`. It is not
recommended to install Perl into a directory containing a space, such as
:file:`C:\\Program Files`. Once the install has completed, log out and log in
again to pick up the changes to the ``PATH`` environment variable.

The Strawberry Perl Windows Installer can be downloaded from the
`Strawberry Perl website <http://strawberryperl.com>`_. Perl will be installed
by default into :file:`C:\\Strawberry`.

One big advantage of Strawberry Perl over ActivePerl is that with Strawberry
Perl, you can use the usual tools available on other OSes to install missing
Perl modules directly from CPAN, whereas ActivePerl requires you to use its own
:file:`ppm` tool to download pre-compiled Perl modules from ActiveState.
The modules in the ActivePerl repository may be a bit older than those on CPAN.

.. _windows-install-bzfiles:

Bugzilla
========

The best way to get Bugzilla is to check it out from git. Download and install
git from the `git website <http://git-scm.com/download>`_, and then run:

:command:`git clone --branch release-X.X-stable https://github.com/bugzilla/bugzilla C:\\bugzilla`

where "X.X" is the 2-digit version number of the stable release of Bugzilla
that you want (e.g. 5.0).

The rest of this documentation assumes you have installed Bugzilla into
:file:`C:\\bugzilla`. Adjust paths appropriately if not.

If it's not possible to use git (e.g. because your Bugzilla machine has no
internet access), you can
`download a tarball of Bugzilla <http://www.bugzilla.org/download/>`_ and
copy it across. Bugzilla comes as a 'tarball' (:file:`.tar.gz` extension),
which any competent Windows archiving tool should be able to open.

.. windows-install-perl-modules:

Perl Modules
============

Bugzilla requires a number of Perl modules to be installed. Some of them are
mandatory, and some others, which enable additional features, are optional.

If you are using ActivePerl, these modules are available in the ActiveState
repository, and are installed with the :file:`ppm` tool. You can either use it
on the command line as below, or just type :command:`ppm`, and you will get a GUI.
If you use a proxy server or a firewall you may have trouble running PPM.
This is covered in the
`ActivePerl FAQ <http://aspn.activestate.com/ASPN/docs/ActivePerl/faq/ActivePerl-faq2.html#ppm_and_proxies>`_.

Install the following mandatory modules with:

:command:`ppm install <modulename>`

* CGI.pm
* Digest-SHA
* TimeDate
* DateTime
* DateTime-TimeZone
* DBI
* Template-Toolkit
* Email-Sender
* Email-MIME
* URI
* List-MoreUtils
* Math-Random-ISAAC
* JSON-XS
* Win32
* Win32-API
* DateTime-TimeZone-Local-Win32

The following modules enable various optional Bugzilla features; try and
install them, but don't worry too much to begin with if you can't get them
installed:

* GD
* Chart
* Template-GD
* GDTextUtil
* GDGraph
* MIME-tools
* libwww-perl
* XML-Twig
* PatchReader
* perl-ldap
* Authen-SASL
* Net-SMTP-SSL
* RadiusPerl
* SOAP-Lite
* XMLRPC-Lite
* JSON-RPC
* Test-Taint
* HTML-Parser
* HTML-Scrubber
* Encode
* Encode-Detect
* Email-Reply
* HTML-FormatText-WithLinks
* TheSchwartz
* Daemon-Generic
* mod_perl
* Apache-SizeLimit
* File-MimeInfo
* IO-stringy
* Cache-Memcached
* File-Copy-Recursive

If you are using Strawberry Perl, you should use the :file:`install-module.pl`
script to install modules, which is the same script used for Linux. Some of
the required modules are already installed by default. The remaining ones can
be installed using the command:

:command:`perl install-module.pl <modulename>`

The list of modules to install will be displayed by :file:`checksetup.pl`; see
below.

.. _windows-config-webserver:

Web Server
==========

Any web server that is capable of running CGI scripts can be made to work.
We have specific instructions for the following:

* :ref:`apache-windows`
* :ref:`iis`

.. windows-config-database:

Database Engine
===============

Bugzilla supports MySQL, PostgreSQL, Oracle and SQLite as database servers.
You only require one of these systems to make use of Bugzilla. MySQL is
most commonly used, and is the only one for which Windows instructions have
been tested. SQLite is good for trial installations as it requires no
setup. Configure your server according to the instructions below:

* :ref:`mysql`
* :ref:`postgresql`
* :ref:`oracle`
* :ref:`sqlite`

.. |checksetupcommand| replace:: :command:`checksetup.pl`
.. |testservercommand| replace:: :command:`testserver.pl http://<your-bugzilla-server>/`

.. include:: installing-end.inc.rst

If you don't see the main Bugzilla page, but instead see "It works!!!",
then somehow your Apache has not picked up your modifications to
:file:`httpd.conf`. If you are on Windows 7 or later, this could be due to a
new feature called "VirtualStore". `This blog post
<http://blog.netscraps.com/bugs/apache-httpd-conf-changes-ignored-in-windows-7.html>`_
may help to solve the problem.

If you get an "Internal Error..." message, it could be that
``ScriptInterpreterSource Registry-Strict`` is not set in your
:ref:`Apache configuration <apache-windows>`. Check again if it is set
properly.

Next, do the :ref:`essential-post-install-config`.
