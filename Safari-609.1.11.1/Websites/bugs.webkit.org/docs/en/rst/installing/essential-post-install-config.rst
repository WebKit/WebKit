.. _essential-post-install-config:

Essential Post-Installation Configuration
#########################################

Bugzilla is configured in the Administration Parameters. Log in with the
administrator account you defined in the last :file:`checksetup.pl` run,
then click :guilabel:`Administration` in the header, and then
:guilabel:`Parameters`. You will see the different parameter sections
down the left hand side of the page.

.. _config-essential-params:

Parameters
==========

There are a few parameters which it is very important to define (or
explicitly decide not to change).

The first set of these are in the :guilabel:`Required Settings` section.

* :param:`urlbase`: this is the URL by which people should access
  Bugzilla's front page.
* :param:`sslbase`: if you have configured SSL on your Bugzilla server,
  this is the SSL URL by which people should access Bugzilla's front page.
* :param:`ssl_redirect`: Set this if you want everyone to be redirected
  to use the SSL version. Recommended if you have set up SSL.
* :param:`cookiepath`: Bugzilla uses cookies to remember who each user is.
  In order to set those cookies in the correct scope, you may need to set a
  cookiepath. If your Bugzilla is at the root of your domain, you don't need
  to change the default value.

You may want to put your email address in the :param:`maintainer`
parameter in the :guilabel:`General` section. This will then let people
know who to contact if they see problems or hit errors.

If you don't want just anyone able to read your Bugzilla, set the
:param:`requirelogin` parameter in the :guilabel:`User Authentication`
section, and change or clear the :param:`createemailregexp` parameter.

.. _email:

Email
=====

Bugzilla requires the ability to set up email. You have a number of choices
here. The simplest is to get Gmail or some other email provider to do the
work for you, but you can also hand the mail off to a local email server,
or run one yourself on the Bugzilla machine.

Bugzilla's approach to email is configured in the :guilabel:`Email` section
of the Parameters.

.. _install-MTA:

Use Another Mail Server
-----------------------

This section corresponds to choosing a :param:`mail_delivery_method` of
:paramval:`SMTP`.

This method passes the email off to an existing mail server. Your
organization may well already have one running for their internal email, and
may prefer to use it for confidentiality reasons. If so, you need the
following information about it:

* The domain name of the server (Parameter: :param:`smtpserver`)
* The username and password to use (Parameters: :param:`smtp_username` and 
  :param:`smtp_password`)
* Whether the server uses SSL (Parameter: :param:`smtp_ssl`)
* The address you should be sending mail 'From' (Parameter:
  :param:`mailfrom`)

If your organization does not run its own mail server, you can use the
services of one of any number of popular email providers.

Gmail
'''''

Visit https://gmail.com and create a new Gmail account for your Bugzilla to
use. Then, set the following parameter values in the "Email" section:

* :param:`mail_delivery_method`: :paramval:`SMTP`
* :param:`mailfrom`: :paramval:`new_gmail_address@gmail.com`
* :param:`smtpserver`: :paramval:`smtp.gmail.com:465`
* :param:`smtp_username`: :paramval:`new_gmail_address@gmail.com`
* :param:`smtp_password`: :paramval:`new_gmail_password`
* :param:`smtp_ssl`: :paramval:`On`

Run Your Own Mail Server
------------------------

This section corresponds to choosing a :param:`mail_delivery_method` of
:paramval:`Sendmail`.

Unless you know what you are doing, and can deal with the possible problems
of spam, bounces and blacklists, it is probably unwise to set up your own
mail server just for Bugzilla. However, if you wish to do so, some guidance
follows.

On Linux, any Sendmail-compatible MTA (Mail Transfer Agent) will
suffice.  Sendmail, Postfix, qmail and Exim are examples of common
MTAs. Sendmail is the original Unix MTA, but the others are easier to
configure, and therefore many people replace Sendmail with Postfix or
Exim. They are drop-in replacements, so Bugzilla will not
distinguish between them.

If you are using Sendmail, version 8.7 or higher is required. If you are
using a Sendmail-compatible MTA, it must be compatible with at least version
8.7 of Sendmail.

On Mac OS X 10.3 and later, `Postfix <http://www.postfix.org/>`_
is used as the built-in email server.  Postfix provides an executable
that mimics sendmail enough to satisfy Bugzilla.

On Windows, if you find yourself unable to use Bugzilla's built-in SMTP
support (e.g. because the necessary Perl modules are not available), you can
use :paramval:`Sendmail` with a little application called
`sendmail.exe <http://glob.com.au/sendmail/>`_, which provides
sendmail-compatible calling conventions and encapsulates the SMTP
communication to another mail server. Like Bugzilla, :command:`sendmail.exe`
can be configured to log SMTP communication to a file in case of problems.

Detailed information on configuring an MTA is outside the scope of this
document. Consult the manual for the specific MTA you choose for detailed
installation instructions. Each of these programs will have their own
configuration files where you must configure certain parameters to
ensure that the mail is delivered properly. They are implemented
as services, and you should ensure that the MTA is in the auto-start
list of services for the machine.

If a simple mail sent with the command-line :file:`mail` program
succeeds, then Bugzilla should also be fine.

Troubleshooting
---------------

If you are having trouble, check that any configured SMTP server can be
reached from your Bugzilla server and that any given authentication
credentials are valid. If these things seem correct and your mails are still
not sending, check if your OS uses SELinux or AppArmor. Either of these
may prevent your web server from sending email. The SELinux boolean
`httpd_can_sendmail <http://selinuxproject.org/page/ApacheRecipes#Allow_the_Apache_HTTP_Server_to_send_mail>`_
may need to be set to True.
   
If all those things don't help, activate the :param:`smtp_debug` parameter
and check your webserver logs.

.. _config-products:

Products, Components, Versions and Milestones
=============================================

Bugs in Bugzilla are categorised into Products and, inside those Products,
Components (and, optionally, if you turn on the :param:`useclassifications`
parameter, Classifications as a level above Products).

Bugzilla comes with a single Product, called "TestProduct", which contains a
single component, imaginatively called "TestComponent". You will want to
create your own Products and their Components. It's OK to have just one
Component inside a Product. Products have Versions (which represents the
version of the software in which a bug was found) and Target Milestones
(which represent the future version of the product in which the bug is
hopefully to be fixed - or, for RESOLVED bugs, was fixed. You may also want
to add some of those.

Once you've created your own, you will want to delete TestProduct (which
will delete TestComponent automatically). Note that if you've filed a bug in
TestProduct to try Bugzilla out, you'll need to move it elsewhere before it's
possible to delete TestProduct.

Now, you may want to do some of the :ref:`optional-post-install-config`.
