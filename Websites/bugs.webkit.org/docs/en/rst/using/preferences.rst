.. _user-preferences:

User Preferences
################

Once logged in, you can customize various aspects of
Bugzilla via the "Preferences" link in the page footer.
The preferences are split into a number of tabs, detailed in the sections
below.

.. _generalpreferences:

General Preferences
===================

This tab allows you to change several default settings of Bugzilla.
Administrators have the power to remove preferences from this list, so you
may not see all the preferences available.

Each preference should be self-explanatory.

.. _emailpreferences:

Email Preferences
=================

This tab allows you to enable or disable email notification on
specific events.

In general, users have almost complete control over how much (or
how little) email Bugzilla sends them. If you want to receive the
maximum amount of email possible, click the ``Enable All
Mail`` button. If you don't want to receive any email from
Bugzilla at all, click the ``Disable All Mail`` button.

.. note:: A Bugzilla administrator can stop a user from receiving
   bugmail by clicking the ``Bugmail Disabled`` checkbox
   when editing the user account. This is a drastic step
   best taken only for disabled accounts, as it overrides
   the user's individual mail preferences.

There are two global options -- ``Email me when someone
asks me to set a flag`` and ``Email me when someone
sets a flag I asked for``. These define how you want to
receive bugmail with regards to flags. Their use is quite
straightforward: enable the checkboxes if you want Bugzilla to
send you mail under either of the above conditions.

If you'd like to set your bugmail to something besides
'Completely ON' and 'Completely OFF', the
``Field/recipient specific options`` table
allows you to do just that. The rows of the table
define events that can happen to a bug -- things like
attachments being added, new comments being made, the
priority changing, etc. The columns in the table define
your relationship with the bug - reporter, assignee, QA contact (if enabled)
or CC list member.

To fine-tune your bugmail, decide the events for which you want
to receive bugmail; then decide if you want to receive it all
the time (enable the checkbox for every column) or only when
you have a certain relationship with a bug (enable the checkbox
only for those columns). For example, if you didn't want to
receive mail when someone added themselves to the CC list, you
could uncheck all the boxes in the ``CC Field Changes``
line. As another example, if you never wanted to receive email
on bugs you reported unless the bug was resolved, you would
uncheck all boxes in the ``Reporter`` column
except for the one on the ``The bug is resolved or
verified`` row.

.. note:: Bugzilla adds the ``X-Bugzilla-Reason`` header to
   all bugmail it sends, describing the recipient's relationship
   (AssignedTo, Reporter, QAContact, CC, or Voter) to the bug.
   This header can be used to do further client-side filtering.

Bugzilla has a feature called ``User Watching``.
When you enter one or more comma-delineated user accounts (usually email
addresses) into the text entry box, you will receive a copy of all the
bugmail those users are sent (security settings permitting).
This powerful functionality enables seamless transitions as developers
change projects or users go on holiday.

Each user listed in the ``Users watching you`` field
has you listed in their ``Users to watch`` list
and can get bugmail according to your relationship to the bug and
their ``Field/recipient specific options`` setting.

Lastly, you can define a list of bugs on which you no longer wish to receive
any email, ever. (You can also add bugs to this list individually by checking
the "Ignore Bug Mail" checkbox on the bug page for that bug.) This is useful
for ignoring bugs where you are the reporter, as that's a role it's not
possible to stop having.

.. _saved-searches:

Saved Searches
==============

On this tab you can view and run any Saved Searches that you have
created, and any Saved Searches that other members of the group
defined in the :param:`querysharegroup` parameter have shared.
Saved Searches can be added to the page footer from this screen.
If somebody is sharing a Search with a group they are allowed to
:ref:`assign users to <groups>`, the sharer may opt to have
the Search show up in the footer of the group's direct members by default.

.. _account-information:

Account Information
===================

On this tab, you can change your basic account information,
including your password, email address and real name. For security
reasons, in order to change anything on this page you must type your
*current* password into the ``Password``
field at the top of the page.
If you attempt to change your email address, a confirmation
email is sent to both the old and new addresses with a link to use to
confirm the change. This helps to prevent account hijacking.

.. _api-keys:

API Keys
========

API keys allow you to give a "token" to some external software so it can log
in to the WebService API as you without knowing your password. You can then
revoke that token if you stop using the web service, and you don't need to
change your password everywhere.

You can create more than one API key if required. Each API key has an optional
description which can help you record what it is used for.

On this page, you can unrevoke, revoke and change the description of existing
API keys for your login. A revoked key means that it cannot be used. The
description is optional and purely for your information.

You can also create a new API key by selecting the checkbox under the 'New
API key' section of the page.

.. _permissions:

Permissions
===========

This is a purely informative page which outlines your current
permissions on this installation of Bugzilla.

A complete list of permissions in a default install of Bugzilla is below.
Your administrator may have defined other permissions. Only users with
*editusers* privileges can change the permissions of other users.

admin
    Indicates user is an Administrator.

bz_canusewhineatothers
    Indicates user can configure whine reports for other users.

bz_canusewhines
    Indicates user can configure whine reports for self.

bz_quip_moderators
    Indicates user can moderate quips.

bz_sudoers
    Indicates user can perform actions as other users.

bz_sudo_protect
    Indicates user cannot be impersonated by other users.

canconfirm
    Indicates user can confirm a bug or mark it a duplicate.

creategroups
    Indicates user can create and destroy groups.

editbugs
    Indicates user can edit all bug fields.

editclassifications
    Indicates user can create, destroy and edit classifications.

editcomponents
    Indicates user can create, destroy and edit products, components,
    versions, milestones and flag types.

editkeywords
    Indicates user can create, destroy and edit keywords.

editusers
    Indicates user can create, disable and edit users.

tweakparams
    Indicates user can change :ref:`Parameters <parameters>`.
