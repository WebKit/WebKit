.. _quick-start:

Quick Start (Ubuntu Linux 22.04)
################################

This quick start guide makes installing Bugzilla as simple as possible for
those who are able to choose their environment. It creates a system using
Ubuntu Linux 22.04 LTS, Apache and MariaDB. It requires a little familiarity
with Linux and the command line.

Obtain Your Hardware
====================

Ubuntu 22.04 LTS Server requires a 64-bit processor.
Bugzilla itself has no prerequisites beyond that, although you should pick
reliable hardware. You can also probably use any 64-bit virtual machine
or cloud instance that you have root access on.

Install the OS
==============

Get `Ubuntu Server 22.04 LTS <https://www.ubuntu.com/download/server>`_
and follow the `installation instructions <https://www.ubuntu.com/download/server/install-ubuntu-server>`_.
Here are some tips:

* Choose any server name you like.
* When creating the initial Linux user, call it ``bugzilla``, give it a 
  strong password, and write that password down.
* You do not need an encrypted lvm group, root or home directory.
* Choose all the defaults for the "partitioning" part (excepting of course
  where the default is "No" and you need to press "Yes" to continue).
* Choose any server name you like.
* When creating the initial Linux user, call it ``bugzilla``, give it a
  strong password, and write that password down.
* From the install options, choose "OpenSSH Server".

Reboot when the installer finishes.

Become root
===========

ssh to the machine as the 'bugzilla' user, or start a console. Then:

:command:`sudo su`

Install Prerequisites
=====================

:command:`apt install git nano`

:command:`apt install apache2 build-essential mariadb-server
libcgi-pm-perl libdigest-sha-perl libtimedate-perl libdatetime-perl
libdatetime-timezone-perl libdbi-perl libtemplate-perl
libemail-address-perl libemail-sender-perl libemail-mime-perl
liburi-perl liblist-moreutils-perl libmath-random-isaac-perl
libjson-xs-perl libgd-perl libchart-perl libtemplate-plugin-gd-perl
libgd-text-perl libgd-graph-perl libmime-tools-perl libwww-perl
libxml-twig-perl libnet-ldap-perl libauthen-sasl-perl
libnet-smtp-ssl-perl libauthen-radius-perl libsoap-lite-perl
libxmlrpc-lite-perl libjson-rpc-perl libtest-taint-perl
libhtml-parser-perl libhtml-scrubber-perl libencode-perl
libencode-detect-perl libemail-reply-perl
libhtml-formattext-withlinks-perl libtheschwartz-perl
libdaemon-generic-perl libapache2-mod-perl2 libapache2-mod-perl2-dev
libfile-mimeinfo-perl libio-stringy-perl libcache-memcached-perl
libfile-copy-recursive-perl libfile-which-perl libdbd-mysql-perl
perlmagick lynx graphviz python3-sphinx rst2pdf`

This will take a little while. It's split into two commands so you can do
the next steps (up to step 7) in another terminal while you wait for the
second command to finish. If you start another terminal, you will need to
:command:`sudo su` again.

Configure MariaDB
=================

The following instructions use the simple :file:`nano` editor, but feel
free to use any text editor you are comfortable with.

:command:`nano /etc/mysql/mariadb.conf.d/50-server.cnf`

Set the following values, which increase the maximum attachment size and
make it possible to search for short words and terms:

* Uncomment and alter on Line 34 to have a value of at least: ``max_allowed_packet=100M``
* Add as new line 42, in the ``[mysqld]`` section: ``ft_min_word_len=2``

Save and exit.

Then, add a user to MySQL for Bugzilla to use:

:command:`mysql -u root -e "GRANT ALL PRIVILEGES ON bugs.* TO bugs@localhost IDENTIFIED BY '$db_pass'"`

Replace ``$db_pass`` with a strong password you have generated. Write it down.
You should make ``$db_pass`` different to your password.

Restart MariaDB:

:command:`service mariadb restart`

Configure Apache
================

:command:`nano /etc/apache2/sites-available/bugzilla.conf`

Paste in the following and save:

.. code-block:: apache

 Alias /bugzilla /var/www/webapps/bugzilla
 <Directory /var/www/webapps/bugzilla>
   AddHandler cgi-script .cgi
   Options +ExecCGI
   DirectoryIndex index.cgi index.html
   AllowOverride All
 </Directory>

This configuration sets up Bugzilla to be served on your server under ``/bugzilla`` path.
For more in depth setup instructions, refer to :ref:`Apache section of this documentation <apache>`.
 
:command:`a2ensite bugzilla`

:command:`a2enmod cgi headers expires rewrite`

:command:`service apache2 restart`

Download Bugzilla
=================

Get it from our Git repository:

:command:`mkdir -p /var/www/webapps`

:command:`cd /var/www/webapps`

:command:`git clone --branch release-X.X-stable https://github.com/bugzilla/bugzilla bugzilla`

(where "X.X" is the 2-digit version number of the stable release of Bugzilla
that you want - e.g. 5.0)

Check Setup
===========

Bugzilla comes with a :file:`checksetup.pl` script which helps with the
installation process. It will need to be run twice. The first time, it
generates a config file (called :file:`localconfig`) for the database
access information, and the second time (step 10)
it uses the info you put in the config file to set up the database.

:command:`cd /var/www/webapps/bugzilla`

:command:`./checksetup.pl`

Edit :file:`localconfig`
========================

:command:`nano localconfig`

You will need to set the following values:

* Line 29: set ``$webservergroup`` to ``www-data``
* Line 67: set ``$db_pass`` to the password for the ``bugs`` user you created
  in MariaDB a few steps ago

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

All the tests should pass. You will get a warning about failing to run
``gdlib-config``; just ignore it.

.. todo:: ``gdlib-config`` is no longer in Ubuntu.

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
have a complex network setup) the address starting with 192 displayed when
you run :command:`hostname -I`.

Accessing Bugzilla from the Internet
====================================

To be able to access Bugzilla from anywhere in the world, you don't have
to make it internet facing at all, there are free VPN services that let
you set up your own network that is accessible anywhere. One of those is
Tailscale, which has a fairly accessible `Quick Start guide <https://tailscale.com/kb/1017/install/>`_.

If you are setting up an internet facing Bugzilla, it's essential to set
up SSL, so that the communication between the server and users is
encrypted. For local and intranet installation this matters less, and
for those cases, you could set up a self signed local certificate
instead.

There are a few ways to set up free SSL thanks to `Let's Encrypt <https://letsencrypt.org/>`_.
The two major ones would be Apache's `mod_md <https://httpd.apache.org/docs/2.4/mod/mod_md.html>`_
and EFF's `certbot <https://certbot.eff.org/instructions?ws=apache&os=ubuntufocal>`_,
but we don't cover the exact specifics of this here, as that's out of
scope of this guide.

Configure Bugzilla
==================

Once you have worked out how to access your Bugzilla in a graphical
web browser, bring up the front page, click :guilabel:`Log In` in the
header, and log in as the admin user you defined in step 10.

Click the :guilabel:`Parameters` link on the page it gives you, and set
the following parameters in the :guilabel:`Required Settings` section:

* :param:`urlbase`:
  :paramval:`http://<servername>/bugzilla/` or :paramval:`http://<ip address>/bugzilla/`
* :param:`ssl_redirect`:
  :paramval:`on` if you set up an SSL certificate

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
