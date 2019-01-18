.. _quick-start:

Quick Start (Ubuntu Linux 14.04)
################################

This quick start guide makes installing Bugzilla as simple as possible for
those who are able to choose their environment. It creates a system using
Ubuntu Linux 14.04 LTS, Apache and MySQL. It requires a little familiarity
with Linux and the command line.

Obtain Your Hardware
====================

Ubuntu 14.04 LTS Server requires a 64-bit processor.
Bugzilla itself has no prerequisites beyond that, although you should pick
reliable hardware. You can also probably use any 64-bit virtual machine
or cloud instance that you have root access on. 

Install the OS
==============

Get `Ubuntu Server 14.04 LTS <http://www.ubuntu.com/download/server>`_
and follow the `installation instructions <http://www.ubuntu.com/download/server/install-ubuntu-server>`_.
Here are some tips:

* Choose any server name you like.
* When creating the initial Linux user, call it ``bugzilla``, give it a 
  strong password, and write that password down.
* You do not need an encrypted home directory.
* Choose all the defaults for the "partitioning" part (excepting of course
  where the default is "No" and you need to press "Yes" to continue).
* Choose "install security updates automatically" unless you want to do
  them manually.
* From the install options, choose "OpenSSH Server" and "LAMP Server".
* Set the password for the MySQL root user to a strong password, and write
  that password down.
* Install the Grub boot loader to the Master Boot Record.

Reboot when the installer finishes.

Become root
===========

ssh to the machine as the 'bugzilla' user, or start a console. Then:

:command:`sudo su`
   
Install Prerequisites
=====================

:command:`apt-get install git nano`

:command:`apt-get install apache2 mysql-server libappconfig-perl libdate-calc-perl libtemplate-perl libmime-perl build-essential libdatetime-timezone-perl libdatetime-perl libemail-sender-perl libemail-mime-perl libemail-mime-modifier-perl libdbi-perl libdbd-mysql-perl libcgi-pm-perl libmath-random-isaac-perl libmath-random-isaac-xs-perl apache2-mpm-prefork libapache2-mod-perl2 libapache2-mod-perl2-dev libchart-perl libxml-perl libxml-twig-perl perlmagick libgd-graph-perl libtemplate-plugin-gd-perl libsoap-lite-perl libhtml-scrubber-perl libjson-rpc-perl libdaemon-generic-perl libtheschwartz-perl libtest-taint-perl libauthen-radius-perl libfile-slurp-perl libencode-detect-perl libmodule-build-perl libnet-ldap-perl libauthen-sasl-perl libtemplate-perl-doc libfile-mimeinfo-perl libhtml-formattext-withlinks-perl libgd-dev libmysqlclient-dev lynx-cur graphviz python-sphinx`

This will take a little while. It's split into two commands so you can do
the next steps (up to step 7) in another terminal while you wait for the
second command to finish. If you start another terminal, you will need to
:command:`sudo su` again.

Download Bugzilla
=================

Get it from our Git repository:

:command:`cd /var/www/html`

:command:`git clone --branch release-X.X-stable https://github.com/bugzilla/bugzilla bugzilla`

(where "X.X" is the 2-digit version number of the stable release of Bugzilla
that you want - e.g. 5.0)

Configure MySQL
===============

The following instructions use the simple :file:`nano` editor, but feel
free to use any text editor you are comfortable with.

:command:`nano /etc/mysql/my.cnf`

Set the following values, which increase the maximum attachment size and
make it possible to search for short words and terms:

* Alter on Line 52: ``max_allowed_packet=100M``
* Add as new line 32, in the ``[mysqld]`` section: ``ft_min_word_len=2``

Save and exit.

Then, add a user to MySQL for Bugzilla to use:

:command:`mysql -u root -p -e "GRANT ALL PRIVILEGES ON bugs.* TO bugs@localhost IDENTIFIED BY '$db_pass'"`

Replace ``$db_pass`` with a strong password you have generated. Write it down.
When you run the above command, it will prompt you for the MySQL root password
that you configured when you installed Ubuntu. You should make ``$db_pass``
different to that password.

Restart MySQL:

:command:`service mysql restart`

Configure Apache
================

:command:`nano /etc/apache2/sites-available/bugzilla.conf`

Paste in the following and save:

.. code-block:: apache

 ServerName localhost

 <Directory /var/www/html/bugzilla>
   AddHandler cgi-script .cgi
   Options +ExecCGI
   DirectoryIndex index.cgi index.html
   AllowOverride All
 </Directory>

:command:`a2ensite bugzilla`

:command:`a2enmod cgi headers expires`

:command:`service apache2 restart`

Check Setup
===========

Bugzilla comes with a :file:`checksetup.pl` script which helps with the
installation process. It will need to be run twice. The first time, it
generates a config file (called :file:`localconfig`) for the database
access information, and the second time (step 10)
it uses the info you put in the config file to set up the database.

:command:`cd /var/www/html/bugzilla`

:command:`./checksetup.pl`

Edit :file:`localconfig`
========================

:command:`nano localconfig`

You will need to set the following values:

* Line 29: set ``$webservergroup`` to ``www-data``
* Line 67: set ``$db_pass`` to the password for the ``bugs`` user you created
  in MySQL a few steps ago

Check Setup (again)
===================

Run the :file:`checksetup.pl` script again to set up the database.

:command:`./checksetup.pl`

It will ask you to give an email address, real name and password for the
first Bugzilla account to be created, which will be an administrator.
Write down the email address and password you set.

Test Server
===========

:command:`./testserver.pl http://localhost/bugzilla`

All the tests should pass. You will get warnings about deprecation from
the ``Chart::Base`` Perl module; just ignore those.

.. todo:: Chart::Base gives confusing deprecation warnings :-|
          https://rt.cpan.org/Public/Bug/Display.html?id=79658 , unfixed for
          2 years. :bug:`1070117`.

Access Via Web Browser
======================

Access the front page:

:command:`lynx http://localhost/bugzilla`

It's not really possible to use Bugzilla for real through Lynx, but you
can view the front page to validate visually that it's up and running.

You might well need to configure your DNS such that the server has, and
is reachable by, a name rather than IP address. Doing so is out of scope
of this document. In the mean time, it is available on your local network
at ``http://<ip address>/bugzilla``, where ``<ip address>`` is (unless you
have a complex network setup) the "inet addr" value displayed when you run
:command:`ifconfig eth0`.

Configure Bugzilla
==================

Once you have worked out how to access your Bugzilla in a graphical
web browser, bring up the front page, click :guilabel:`Log In` in the
header, and log in as the admin user you defined in step 10.

Click the :guilabel:`Parameters` link on the page it gives you, and set
the following parameters in the :guilabel:`Required Settings` section:

* :param:`urlbase`:
  :paramval:`http://<servername>/bugzilla/` or :paramval:`http://<ip address>/bugzilla/`

Click :guilabel:`Save Changes` at the bottom of the page.

There are several ways to get Bugzilla to send email. The easiest is to
use Gmail, so we do that here so you have it working. Visit
https://gmail.com and create a new Gmail account for your Bugzilla to use.
Then, open the :guilabel:`Email` section of the Parameters using the link
in the left column, and set the following parameter values:

* :param:`mail_delivery_method`: :paramval:`SMTP`
* :param:`mailfrom`: :paramval:`new_gmail_address@gmail.com`
* :param:`smtpserver`: :paramval:`smtp.gmail.com:465`
* :param:`smtp_username`: :paramval:`new_gmail_address@gmail.com`
* :param:`smtp_password`: :paramval:`new_gmail_password`
* :param:`smtp_ssl`: :paramval:`On`

Click :guilabel:`Save Changes` at the bottom of the page.

And you're all ready to go. :-)
