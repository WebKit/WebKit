Groups
======

The API for creating, changing, and getting information about groups.

.. _rest_group_create:

Create Group
------------

This allows you to create a new group in Bugzilla. You must be authenticated and
be in the *creategroups* group to perform this action.

**Request**

.. code-block:: text

   POST /rest/group

.. code-block:: js

   {
     "name" : "secret-group",
     "description" : "Too secret for you!",
     "is_active" : true
   }

Some params must be set, or an error will be thrown. The required params are
marked in **bold**.

===============  =======  =======================================================
name             type     description
===============  =======  =======================================================
**name**         string   A short name for this group. Must be unique. This
                          is not usually displayed in the user interface, except
                          in a few places.
**description**  string   A human-readable name for this group. Should be
                          relatively short. This is what will normally appear in
                          the UI as the name of the group.
user_regexp      string   A regular expression. Any user whose Bugzilla username
                          matches this regular expression will automatically be
                          granted membership in this group.
is_active        boolean  ``true`` if new group can be used for bugs, ``false``
                          if this is a group that will only contain users and no
                          bugs will be restricted to it.
icon_url         string   A URL pointing to a small icon used to identify the
                          group. This icon will show up next to users' names in
                          various parts of Bugzilla if they are in this group.
===============  =======  =======================================================

**Response**

.. code-block:: js

   {
     "id": 22
   }

====  ====  ==============================
name  type  description
====  ====  ==============================
id    int   ID of the newly-created group.
====  ====  ==============================

.. _rest_group_update:

Update Group
------------

This allows you to update a group in Bugzilla. You must be authenticated and be
in the *creategroups* group to perform this action.

**Request**

To update a group using the group ID or name:

.. code-block:: text

   PUT /rest/group/(id_or_name)

.. code-block:: js

   {
     "name" : "secret-group",
     "description" : "Too secret for you! (updated description)",
     "is_active" : false
   }

You can edit a single group by passing the ID or name of the group
in the URL. To edit more than one group, you can specify addition IDs or
group names using the ``ids`` or ``names`` parameters respectively.

One of the below must be specified.

==============  =====  ==========================================================
name            type   description
==============  =====  ==========================================================
**id_or_name**  mixed  Integer group or name.
**ids**         array  IDs of groups to update.
**names**       array  Names of groups to update.
==============  =====  ==========================================================

The following parameters specify the new values you want to set for the group(s)
you are updating.

===========  =======  ===========================================================
name         type     description
===========  =======  ===========================================================
name         string   A new name for the groups. If you try to set this while
                      updating more than one group, an error will occur, as
                      group names must be unique.
description  string   A new description for the groups. This is what will appear
                      in the UI as the name of the groups.
user_regexp  string   A new regular expression for email. Will automatically
                      grant membership to these groups to anyone with an email
                      address that matches this perl regular expression.
is_active    boolean  Set if groups are active and eligible to be used for bugs.
                      ``true`` if bugs can be restricted to this group, ``false``
                      otherwise.
icon_url     string   A URL pointing to an icon that will appear next to the name
                      of users who are in this group.
===========  =======  ===========================================================

**Response**

.. code-block:: js

  {
    "groups": [
      {
        "changes": {
          "description": {
            "added": "Too secret for you! (updated description)",
            "removed": "Too secret for you!"
          },
          "is_active": {
            "removed": "1",
            "added": "0"
          }
        },
        "id": "22"
      }
    ]
  }

``groups`` (array) Group change objects, each containing the following items:

=======  ======  ================================================================
name     type    description
=======  ======  ================================================================
id       int     The ID of the group that was updated.
changes  object  The changes that were actually done on this group. The
                 keys are the names of the fields that were changed, and the
                 values are an object with two items:

                 * added: (string) The values that were added to this field,
                   possibly a comma-and-space-separated list if multiple values
                   were added.
                 * removed: (string) The values that were removed from this
                   field, possibly a comma-and-space-separated list if multiple
                   values were removed.
=======  ======  ================================================================

.. _rest_group_get:

Get Group
---------

Returns information about Bugzilla groups.

**Request**

To return information about a specific group ID or name:

.. code-block:: text

   GET /rest/group/(id_or_name)

You can also return information about more than one specific group by using the
following in your query string:

.. code-block:: text

   GET /rest/group?ids=1&ids=2&ids=3
   GET /group?names=ProductOne&names=Product2

If neither IDs nor names are passed, and you are in the creategroups or
editusers group, then all groups will be retrieved. Otherwise, only groups
that you have bless privileges for will be returned.

==========  =======  ============================================================
name        type     description
==========  =======  ============================================================
id_or_name  mixed    Integer group ID or name.
ids         array    Integer IDs of groups.
names       array    Names of groups.
membership  boolean  Set to 1 then a list of members of the passed groups names
                     and IDs will be returned.
==========  =======  ============================================================

**Response**

.. code-block:: js

   {
     "groups": [
       {
         "membership": [
           {
             "real_name": "Bugzilla User",
             "can_login": true,
             "name": "user@bugzilla.org",
             "login_denied_text": "",
             "id": 85,
             "email_enabled": false,
             "email": "user@bugzilla.org"
           },
         ],
         "is_active": true,
         "description": "Test Group",
         "user_regexp": "",
         "is_bug_group": true,
         "name": "TestGroup",
         "id": 9
       }
     ]
   }

If the user is a member of the *creategroups* group they will receive
information about all groups or groups matching the criteria that they passed.
You have to be in the creategroups group unless you're requesting membership
information.

If the user is not a member of the *creategroups* group, but they are in the
"editusers" group or have bless privileges to the groups they require
membership information for, the is_active, is_bug_group and user_regexp values
are not supplied.

The return value will be an object containing group names as the keys; each
value will be an object that describes the group and has the following items:

============  ======  ===========================================================
name          type    description
============  ======  ===========================================================
id            int     The unique integer ID that Bugzilla uses to identify this
                      group. Even if the name of the group changes, this ID will
                      stay the same.
name          string  The name of the group.
description   string  The description of the group.
is_bug_group  int     Whether this group is to be used for bug reports or is
                      only administrative specific.
user_regexp   string  A regular expression that allows users to be added to
                      this group if their login matches.
is_active     int     Whether this group is currently active or not.
users         array   User objects that are members of this group; only
                      returned if the user sets the ``membership`` parameter to
                      1. Each user object has the items describe in the User
                      object below.
============  ======  ===========================================================

User object:

=============  =======  =========================================================
name           type     description
=============  =======  =========================================================
id             int      The ID of the user.
real_name      string   The actual name of the user.
email          string   The email address of the user.
name           string   The login name of the user. Note that in some situations
                        this is different than their email.
can_login      boolean  A boolean value to indicate if the user can login into
                        bugzilla.
email_enabled  boolean  A boolean value to indicate if bug-related mail will
                        be sent to the user or not.
disabled_text  string   A text field that holds the reason for disabling a user
                        from logging into Bugzilla. If empty, then the user
                        account is enabled; otherwise it is disabled/closed.
=============  =======  =========================================================
