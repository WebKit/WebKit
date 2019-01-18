.. _mac-os-x:

Mac OS X
########

.. _macosx-install-packages:

.. note:: The Bugzilla team has very little Mac expertise and we've not been
          able to do a successful install of the latest version. We got
          close, though. If you've managed it, tell us how and we can update
          these docs!

Install Packages
================

OS X 10.7 provides Perl 5.12 and Apache 2.2. Install the following additional
packages:

* git: Download an installer from
  `the git website <http://www.git-scm.com/downloads>`_. 
* MySQL: Download an installer from
  `the MySQL website <http://dev.mysql.com/downloads/mysql/>`_.

.. _macosx-install-bzfiles:

Bugzilla
========

The best way to get Bugzilla is to check it out from git:

:command:`git clone --branch release-X.X-stable https://github.com/bugzilla/bugzilla`

Run the above command in your home directory, replacing "X.X" with the 2-digit
version number of the stable release of Bugzilla that you want - e.g. "4.4".
This will place Bugzilla in the directory :file:`$HOME/bugzilla`.

If that's not possible, you can
`download a tarball of Bugzilla <http://www.bugzilla.org/download/>`_.

.. _macosx-libraries:

Additional System Libraries
===========================

Apple does not include the GD library with Mac OS X. Bugzilla needs this if
you want to display bug graphs, and you need to install it before you try
installing the GD Perl module.

You can use `MacPorts <http://www.macports.org/>`_, `Homebrew <http://brew.sh/>`_ or
`Fink <http://sourceforge.net/projects/fink/>`_, all of which can install common
Unix programs on Mac OS X.

If you don't have one of the above installed already, pick one and follow the
instructions for setting it up. Then, use it to install the :file:`gd2` package
(MacPorts/Fink) or the :file:`gd` package (Brew).

The package manager may prompt you to install a number of dependencies; you
will need to agree to this.

.. note:: To prevent creating conflicts with the software that Apple
   installs by default, Fink creates its own directory tree at :file:`/sw`
   where it installs most of
   the software that it installs. This means your libraries and headers
   will be at :file:`/sw/lib` and :file:`/sw/include` instead
   of :file:`/usr/lib` and :file:`/usr/include`. When the
   Perl module config script for the GD module asks where your :file:`libgd`
   is, be sure to tell it :file:`/sw/lib`.

.. _macosx-install-perl-modules:

Perl Modules
============

Bugzilla requires a number of Perl modules. On Mac OS X, the easiest thing to
do is to install local copies (rather than system-wide copies) of any ones
that you don't already have. However, if you do want to install them
system-wide, run the below commands as root with the :command:`--global`
option.

To check whether you have all the required modules and what is still missing,
run:

:command:`perl checksetup.pl --check-modules`

You can run this command as many times as necessary.

Install all missing modules locally like this:

:command:`perl install-module.pl --all`

.. _macosx-config-webserver:

Web Server
==========

Any web server that is capable of running CGI scripts can be made to work.
We have specific configuration instructions for the following:

* :ref:`apache`

You'll need to create a symbolic link so the webserver can see Bugzilla:

:command:`cd /Library/WebServer/Documents`

:command:`sudo ln -s $HOME/bugzilla bugzilla`

In :guilabel:`System Preferences` --> :guilabel:`Sharing`, enable the
:guilabel:`Web Sharing` checkbox to start Apache. 

.. _macosx-config-database:

Database Engine
===============

Bugzilla supports MySQL, PostgreSQL, Oracle and SQLite as database servers.
You only require one of these systems to make use of Bugzilla. MySQL is
most commonly used on Mac OS X. (In fact, we have no reports of anyone using
anything else.) Configure your server according to the instructions below:

* :ref:`mysql`
* :ref:`postgresql`
* :ref:`oracle`
* :ref:`sqlite`

.. |checksetupcommand| replace:: :command:`perl checksetup.pl`
.. |testservercommand| replace:: :command:`perl testserver.pl http://<your-bugzilla-server>/`

.. include:: installing-end.inc.rst

Next, do the :ref:`essential-post-install-config`.
