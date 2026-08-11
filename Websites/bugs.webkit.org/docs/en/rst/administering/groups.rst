.. _groups:

Groups and Security
###################

Groups allow for separating bugs into logical divisions.
Groups are typically used
to isolate bugs that should only be seen by certain people. For
example, a company might create a different group for each one of its customers
or partners. Group permissions could be set so that each partner or customer would
only have access to their own bugs. Or, groups might be used to create
variable access controls for different departments within an organization.
Another common use of groups is to associate groups with products,
creating isolation and access control on a per-product basis.

Groups and group behaviors are controlled in several places:

#. The group configuration page. To view or edit existing groups, or to
   create new groups, access the "Groups" link from the "Administration"
   page. This section of the manual deals primarily with the aspect of
   group controls accessed on this page.

#. Global configuration parameters. Bugzilla has several parameters
   that control the overall default group behavior and restriction
   levels. For more information on the parameters that control
   group behavior globally, see :ref:`param-group-security`.

#. Product association with groups. Most of the functionality of groups
   and group security is controlled at the product level. Some aspects
   of group access controls for products are discussed in this section,
   but for more detail see :ref:`product-group-controls`.

#. Group access for users. See :ref:`users-and-groups` for
   details on how users are assigned group access.

Group permissions are such that if a bug belongs to a group, only members
of that group can see the bug. If a bug is in more than one group, only
members of *all* the groups that the bug is in can see
the bug. For information on granting read-only access to certain people and
full edit access to others, see :ref:`product-group-controls`.

.. note:: By default, bugs can also be seen by the Assignee, the Reporter, and
   everyone on the CC List, regardless of whether or not the bug would
   typically be viewable by them. Visibility to the Reporter and CC List can
   be overridden (on a per-bug basis) by bringing up the bug, finding the
   section that starts with ``Users in the roles selected below...``
   and un-checking the box next to either 'Reporter' or 'CC List' (or both).

.. _create-groups:

Creating Groups
===============

To create a new group, follow the steps below:

#. Select the ``Administration`` link in the page footer,
   and then select the ``Groups`` link from the
   Administration page.

#. A table of all the existing groups is displayed. Below the table is a
   description of all the fields. To create a new group, select the
   ``Add Group`` link under the table of existing groups.

#. There are five fields to fill out. These fields are documented below
   the form. Choose a name and description for the group. Decide whether
   this group should be used for bugs (in all likelihood this should be
   selected). Optionally, choose a regular expression that will
   automatically add any matching users to the group, and choose an
   icon that will help identify user comments for the group. The regular
   expression can be useful, for example, to automatically put all users
   from the same company into one group (if the group is for a specific
   customer or partner).

   .. note:: If ``User RegExp`` is filled out, users whose email
      addresses match the regular expression will automatically be
      members of the group as long as their email addresses continue
      to match the regular expression. If their email address changes
      and no longer matches the regular expression, they will be removed
      from the group. Versions 2.16 and older of Bugzilla did not automatically
      remove users whose email addresses no longer matched the RegExp.

   .. warning:: If specifying a domain in the regular expression, end
      the regexp with a "$". Otherwise, when granting access to
      "@mycompany\\.com", access will also be granted to
      'badperson@mycompany.com.cracker.net'. Use the syntax,
      '@mycompany\\.com$' for the regular expression.

#. After the new group is created, it can be edited for additional options.
   The "Edit Group" page allows for specifying other groups that should be included
   in this group and which groups should be permitted to add and delete
   users from this group. For more details, see :ref:`edit-groups`.

.. _edit-groups:

Editing Groups and Assigning Group Permissions
==============================================

To access the "Edit Groups" page, select the
``Administration`` link in the page footer,
and then select the ``Groups`` link from the Administration page.
A table of all the existing groups is displayed. Click on a group name
you wish to edit or control permissions for.

The "Edit Groups" page contains the same five fields present when
creating a new group. Below that are two additional sections, "Group
Permissions" and "Mass Remove". The "Mass Remove" option simply removes
all users from the group who match the regular expression entered. The
"Group Permissions" section requires further explanation.

The "Group Permissions" section on the "Edit Groups" page contains four sets
of permissions that control the relationship of this group to other
groups. If the :param:`usevisibilitygroups` parameter is in use (see
:ref:`parameters`) two additional sets of permissions are displayed.
Each set consists of two select boxes. On the left, a select box
with a list of all existing groups. On the right, a select box listing
all groups currently selected for this permission setting (this box will
be empty for new groups). The way these controls allow groups to relate
to one another is called *inheritance*.
Each of the six permissions is described below.

*Groups That Are a Member of This Group*
    Members of any groups selected here will automatically have
    membership in this group. In other words, members of any selected
    group will inherit membership in this group.

*Groups That This Group Is a Member Of*
    Members of this group will inherit membership to any group
    selected here. For example, suppose the group being edited is
    an Admin group. If there are two products  (Product1 and Product2)
    and each product has its
    own group (Group1 and Group2), and the Admin group
    should have access to both products,
    simply select both Group1 and Group2 here.

*Groups That Can Grant Membership in This Group*
    The members of any group selected here will be able add users
    to this group, even if they themselves are not in this group.

*Groups That This Group Can Grant Membership In*
    Members of this group can add users to any group selected here,
    even if they themselves are not in the selected groups.

*Groups That Can See This Group*
    Members of any selected group can see the users in this group.
    This setting is only visible if the :param:`usevisibilitygroups` parameter
    is enabled on the Bugzilla Configuration page. See
    :ref:`parameters` for information on configuring Bugzilla.

*Groups That This Group Can See*
    Members of this group can see members in any of the selected groups.
    This setting is only visible if the :param:`usevisibilitygroups` parameter
    is enabled on the the Bugzilla Configuration page. See
    :ref:`parameters` for information on configuring Bugzilla.

.. _users-and-groups:

Assigning Users to Groups
=========================

A User can become a member of a group in several ways:

#. The user can be explicitly placed in the group by editing
   the user's profile. This can be done by accessing the "Users" page
   from the "Administration" page. Use the search form to find the user
   you want to edit group membership for, and click on their email
   address in the search results to edit their profile. The profile
   page lists all the groups and indicates if the user is a member of
   the group either directly or indirectly. More information on indirect
   group membership is below. For more details on User Administration,
   see :ref:`users`.

#. The group can include another group of which the user is
   a member. This is indicated by square brackets around the checkbox
   next to the group name in the user's profile.
   See :ref:`edit-groups` for details on group inheritance.

#. The user's email address can match the regular expression
   that has been specified to automatically grant membership to
   the group. This is indicated by "\*" around the check box by the
   group name in the user's profile.
   See :ref:`create-groups` for details on
   the regular expression option when creating groups.

Assigning Group Controls to Products
====================================

The primary functionality of groups is derived from the relationship of
groups to products. The concepts around segregating access to bugs with
product group controls can be confusing. For details and examples on this
topic, see :ref:`product-group-controls`.

