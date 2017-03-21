.. _users:

Users
#####

.. _defaultuser:

Creating Admin Users
====================

When you first run checksetup.pl after installing Bugzilla, it will
prompt you for the username (email address) and password for the first
admin user. If for some reason you delete all the admin users,
re-running checksetup.pl will again prompt you for a username and
password and make a new admin.

If you wish to add more administrative users, add them to the "admin" group.

.. _user-account-search:

Searching For Users
===================

If you have ``editusers`` privileges or if you are allowed
to grant privileges for some groups, the :guilabel:`Users` link
will appear in the Administration page.

The first screen is a search form to search for existing user
accounts. You can run searches based either on the user ID, real
name or login name (i.e. the email address, or just the first part
of the email address if the :param:`emailsuffix` parameter is set).
The search can be conducted
in different ways using the listbox to the right of the text entry
box. You can match by case-insensitive substring (the default),
regular expression, a *reverse* regular expression
match (which finds every user name which does NOT match the regular
expression), or the exact string if you know exactly who you are
looking for. The search can be restricted to users who are in a
specific group. By default, the restriction is turned off.

The search returns a list of
users matching your criteria. User properties can be edited by clicking
the login name. The Account History of a user can be viewed by clicking
the "View" link in the Account History column. The Account History
displays changes that have been made to the user account, the time of
the change and the user who made the change. For example, the Account
History page will display details of when a user was added or removed
from a group.

.. _modifyusers:

Modifying Users
===============

Once you have found your user, you can change the following
fields:

- *Login Name*:
  This is generally the user's full email address. However, if you
  have are using the :param:`emailsuffix` parameter, this may
  just be the user's login name. Unless you turn off the
  :param:`allowemailchange` parameter, users can change their
  login names themselves (to any valid email address).

- *Real Name*: The user's real name. Note that
  Bugzilla does not require this to create an account.

- *Password*:
  You can change the user's password here. Users can automatically
  request a new password, so you shouldn't need to do this often.
  If you want to disable an account, see Disable Text below.

- *Bugmail Disabled*:
  Mark this checkbox to disable bugmail and whinemail completely
  for this account. This checkbox replaces the data/nomail file
  which existed in older versions of Bugzilla.

- *Disable Text*:
  If you type anything in this box, including just a space, the
  user is prevented from logging in and from making any changes to
  bugs via the web interface.
  The HTML you type in this box is presented to the user when
  they attempt to perform these actions and should explain
  why the account was disabled.
  Users with disabled accounts will continue to receive
  mail from Bugzilla; furthermore, they will not be able
  to log in themselves to change their own preferences and
  stop it. If you want an account (disabled or active) to
  stop receiving mail, simply check the
  ``Bugmail Disabled`` checkbox above.

  .. note:: Even users whose accounts have been disabled can still
     submit bugs via the email gateway, if one exists.
     The email gateway should *not* be
     enabled for secure installations of Bugzilla.

  .. warning:: Don't disable all the administrator accounts!

- *<groupname>*:
  If you have created some groups, e.g. "securitysensitive", then
  checkboxes will appear here to allow you to add users to, or
  remove them from, these groups. The first checkbox gives the
  user the ability to add and remove other users as members of
  this group. The second checkbox adds the user himself as a member
  of the group.

- *canconfirm*:
  This field is only used if you have enabled the "unconfirmed"
  status. If you enable this for a user,
  that user can then move bugs from "Unconfirmed" to a "Confirmed"
  status (e.g.: "New" status).

- *creategroups*:
  This option will allow a user to create and destroy groups in
  Bugzilla.

- *editbugs*:
  Unless a user has this bit set, they can only edit those bugs
  for which they are the assignee or the reporter. Even if this
  option is unchecked, users can still add comments to bugs.

- *editcomponents*:
  This flag allows a user to create new products and components,
  modify existing products and components, and destroy those that have
  no bugs associated with them. If a product or component has bugs
  associated with it, those bugs must be moved to a different product
  or component before Bugzilla will allow them to be destroyed.

- *editkeywords*:
  If you use Bugzilla's keyword functionality, enabling this
  feature allows a user to create and destroy keywords. A keyword
  must be removed from any bugs upon which it is currently set
  before it can be destroyed.

- *editusers*:
  This flag allows a user to do what you're doing right now: edit
  other users. This will allow those with the right to do so to
  remove administrator privileges from other users or grant them to
  themselves. Enable with care.

- *tweakparams*:
  This flag allows a user to change Bugzilla's Params
  (using :file:`editparams.cgi`.)

- *<productname>*:
  This allows an administrator to specify the products
  in which a user can see bugs. If you turn on the
  :param:`makeproductgroups` parameter in
  the Group Security Panel in the Parameters page,
  then Bugzilla creates one group per product (at the time you create
  the product), and this group has exactly the same name as the
  product itself. Note that for products that already exist when
  the parameter is turned on, the corresponding group will not be
  created. The user must still have the :group:`editbugs`
  privilege to edit bugs in these products.

.. _createnewusers:

Creating New Users
==================

.. _self-registration:

Self-Registration
-----------------

By default, users can create their own user accounts by clicking the
``New Account`` link at the bottom of each page (assuming
they aren't logged in as someone else already). If you want to disable
this self-registration, or if you want to restrict who can create their
own user account, you have to edit the :param:`createemailregexp`
parameter in the ``Configuration`` page; see
:ref:`parameters`.

.. _user-account-creation:

Administrator Registration
--------------------------

Users with ``editusers`` privileges, such as administrators,
can create user accounts for other users:

#. After logging in, click the "Users" link at the footer of
   the query page, and then click "Add a new user".

#. Fill out the form presented. This page is self-explanatory.
   When done, click "Submit".

   .. note:: Adding a user this way will *not*
      send an email informing them of their username and password.
      While useful for creating dummy accounts (watchers which
      shuttle mail to another system, for instance, or email
      addresses which are a mailing list), in general it is
      preferable to log out and use the ``New Account``
      button to create users, as it will pre-populate all the
      required fields and also notify the user of her account name
      and password.

.. _user-account-deletion:

Deleting Users
==============

If the :param:`allowuserdeletion` parameter is turned on (see
:ref:`parameters`) then you can also delete user accounts.
Note that, most of the time, this is not the best thing to do. If only
a warning in a yellow box is displayed, then the deletion is safe.
If a warning is also displayed in a red box, then you should NOT try
to delete the user account, else you will get referential integrity
problems in your database, which can lead to unexpected behavior,
such as bugs not appearing in bug lists anymore, or data displaying
incorrectly. You have been warned!

.. _impersonatingusers:

Impersonating Users
===================

There may be times when an administrator would like to do something as
another user.  The :command:`sudo` feature may be used to do
this.

.. note:: To use the sudo feature, you must be in the
   *bz_sudoers* group.  By default, all
   administrators are in this group.

If you have access to this feature, you may start a session by
going to the Edit Users page, Searching for a user and clicking on
their login.  You should see a link below their login name titled
"Impersonate this user".  Click on the link.  This will take you
to a page where you will see a description of the feature and
instructions for using it.  After reading the text, simply
enter the login of the user you would like to impersonate, provide
a short message explaining why you are doing this, and press the
button.

As long as you are using this feature, everything you do will be done
as if you were logged in as the user you are impersonating.

.. warning:: The user you are impersonating will not be told about what you are
   doing.  If you do anything that results in mail being sent, that
   mail will appear to be from the user you are impersonating.  You
   should be extremely careful while using this feature.

