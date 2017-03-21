Flag Types
==========

This part of the Bugzilla API allows you to get and create bug and attachment
flags.

.. _rest_flagtype_get:

Get Flag Type
-------------

Get information about valid flag types that can be set for bugs and attachments.

**Request**

To get information about all flag types for a product:

.. code-block:: text

   GET /rest/flag_type/(product)

To get information about flag_types for a product and component:

.. code-block:: text

   GET /rest/flag_type/(product)/(component)

.. code-block:: js

   {
     "bug": [
       {
         "is_multiplicable": false,
         "is_requesteeble": false,
         "values": [
           "X",
           "?",
           "+",
           "-"
         ],
         "id": 11,
         "type": "bug",
         "is_active": true,
         "description": "Blocks the next release",
         "name": "blocker"
       },
       {
         "is_requesteeble": false,
         "is_multiplicable": false,
         "is_active": true,
         "description": "Regression found?",
         "name": "regression",
         "id": 10,
         "type": "bug",
         "values": [
           "X",
           "?",
           "+",
           "-"
         ]
       },
     ],
     "attachment": [
       {
         "is_requesteeble": true,
         "is_multiplicable": true,
         "name": "review",
         "is_active": true,
         "description": "Review the patch for correctness and applicability to the problem.",
         "type": "attachment",
         "id": 1,
         "values": [
           "X",
           "?",
           "+",
           "-"
         ]
       },
       {
         "name": "approval",
         "description": "Approve the patch for check-in to the tree.",
         "is_active": true,
         "values": [
           "X",
           "?",
           "+",
           "-"
         ],
         "type": "attachment",
         "id": 3,
         "is_multiplicable": false,
         "is_requesteeble": false
       }
     ]
   }

You must pass a product name and an optional component name. If the product or
component names contains a ``/`` character, up will need to url encode it.

===========  ======  ============================================================
name         type    description
===========  ======  ============================================================
**product**  string  The name of a valid product.
component    string  An optional valid component name associated with the
                     product.
===========  ======  ============================================================

**Response**

An object containing two items, ``bug`` and ``attachment``. Each value is an
array of objects, containing the following items:

================  =======  ======================================================
name              type     description
================  =======  ======================================================
id                int      An integer ID uniquely identifying this flag type.
name              string   The name for the flag type.
type              string   The target of the flag type which is either ``bug``
                           or ``attachment``.
description       string   The description of the flag type.
values            array    String values that the user can set on the flag type.
is_requesteeble   boolean  Users can ask specific other users to set flags of
                           this type.
is_multiplicable  boolean  Multiple flags of this type can be set for the same
                           bug or attachment.
================  =======  ======================================================

Create Flag Type
----------------

Create a new flag type. You must be authenticated and be in the *editcomponents*
group to perform this action.

**Request**

.. code-block:: text

   POST /rest/flag_type

.. code-block:: js

   {
     "name" : "feedback",
     "description" : "This attachment needs feedback",
     "inclusions" : [ "WorldControl "],
     "target_type" : "attachment"
   }

Some params must be set, or an error will be thrown. The required params are
marked in **bold**.

===========================  =======  ===========================================
name                         type     description
===========================  =======  ===========================================
**name**                     string   The name of the new flag type.
**description**              string   A description for the flag type.
target_type                  string   The new flag is either for a ``bug`` or an
                                      ``attachment``.
inclusions                   array    An array of strings or an object containing
                                      product names, and optionally component
                                      names. If you provide a string, the flag
                                      type will be shown on all bugs in that
                                      product. If you provide an object, the key
                                      represents the product name, and the value
                                      is the components of the product to be
                                      included.
exclusions                   array    An array of strings or an object containing
                                      product names. This uses the same format as
                                      ``inclusions``. This will exclude the flag
                                      from all products and components specified.
sortkey                      int      A number between 1 and 32767 by which this
                                      type will be sorted when displayed to users
                                      in a list; ignore if you don't care what
                                      order the types appear in or if you want
                                      them to appear in alphabetical order.
is_active                    boolean  Flag of this type appear in the UI and can
                                      be set. Default is ``true``.
is_requestable               boolean  Users can ask for flags of this type to be
                                      set. Default is ``true``.
cc_list                      array    If the flag type is requestable, who should
                                      receive e-mail notification of requests.
                                      This is an array of e-mail addresses which\
                                      do not need to be Bugzilla logins.
is_specifically_requestable  boolean  Users can ask specific other users to
                                      set flags of this type as opposed to just
                                      asking the wind. Default is ``true``.
is_multiplicable             boolean  Multiple flags of this type can be set on
                                      the same bug. Default is ``true``.
grant_group                  string   The group allowed to grant/deny flags of
                                      this type (to allow all users to grant/deny
                                      these flags, select no group). Default is
                                      no group.
request_group                string   If flags of this type are requestable, the
                                      group allowed to request them (to allow all
                                      users to request these flags, select no
                                      group). Note that the request group alone
                                      has no effect if the grant group is not
                                      defined! Default is no group.
===========================  =======  ===========================================

An example for ``inclusions`` and/or ``exclusions``:

.. code-block:: js

   [
     "FooProduct"
   ]

   {
     "BarProduct" : [ "C1", "C3" ],
     "BazProduct" : [ "C7" ]
   }

This flag will be added to **all** components of ``FooProduct``, components C1
and C3 of ``BarProduct``, and C7 of ``BazProduct``.

**Response**

.. code-block:: js

   {
     "id": 13
   }

=======  ====  ==============================================
name     type  description
=======  ====  ==============================================
flag_id  int   ID of the new FlagType object is returned.
=======  ====  ==============================================

.. _rest_flagtype_update:

Update Flag Type
----------------

This allows you to update a flag type in Bugzilla. You must be authenticated
and be in the *editcomponents* group to perform this action.

**Request**

.. code-block:: text

   PUT /rest/flag_type/(id_or_name)

.. code-block:: js

   {
     "ids" : [13],
     "name" : "feedback-new",
     "is_requestable" : false
   }

You can edit a single flag type by passing the ID or name of the flag type
in the URL. To edit more than one flag type, you can specify addition IDs or
flag type names using the ``ids`` or ``names`` parameters respectively.

One of the below must be specified.

==============  =====  ==========================================================
name            type   description
==============  =====  ==========================================================
**id_or_name**  mixed  Integer flag type ID or name.
**ids**         array  Numeric IDs of the flag types that you wish to update.
**names**       array  Names of the flag types that you wish to update. If many
                       flag types have the same name, this will change **all**
                       of them.
==============  =====  ==========================================================

The following parameters specify the new values you want to set for the flag
types you are updating.

===========================  =======  ===========================================
name                         type     description
===========================  =======  ===========================================
name                         string   A short name identifying this type.
description                  string   A comprehensive description of this type.
inclusions                   array    An array of strings or an object containing
                                      product names, and optionally component
                                      names. If you provide a string, the flag
                                      type will be shown on all bugs in that
                                      product. If you provide an object, the key
                                      represents the product name, and the value
                                      is the components of the product to be
                                      included.
exclusions                   array    An array of strings or an object containing
                                      product names. This uses the same format as
                                      ``inclusions``. This will exclude the flag
                                      from all products and components specified.
sortkey                      int      A number between 1 and 32767 by which this
                                      type will be sorted when displayed to users
                                      in a list; ignore if you don't care what
                                      order the types appear in or if you want
                                      them to appear in alphabetical order.
is_active                    boolean  Flag of this type appear in the UI and can
                                      be set.
is_requestable               boolean  Users can ask for flags of this type to be
                                      set.
cc_list                      array    If the flag type is requestable, who should
                                      receive e-mail notification of requests.
                                      This is an array of e-mail addresses
                                      which do not need to be Bugzilla logins.
is_specifically_requestable  boolean  Users can ask specific other users to set
                                      flags of this type as opposed to just
                                      asking the wind.
is_multiplicable             boolean  Multiple flags of this type can be set on
                                      the same bug.
grant_group                  string   The group allowed to grant/deny flags of
                                      this type (to allow all users to grant/deny
                                      these flags, select no group).
request_group                string   If flags of this type are requestable, the
                                      group allowed to request them (to allow all
                                      users to request these flags, select no
                                      group). Note that the request group alone
                                      has no effect if the grant group is not
                                      defined!
===========================  =======  ===========================================

An example for ``inclusions`` and/or ``exclusions``:

.. code-block:: js

   [
     "FooProduct",
   ]

   {
     "BarProduct" : [ "C1", "C3" ],
     "BazProduct" : [ "C7" ]
   }

This flag will be added to **all** components of ``FooProduct``,
components C1 and C3 of ``BarProduct``, and C7 of ``BazProduct``.

**Response**

.. code-block:: js

   {
     "flagtypes": [
       {
         "name": "feedback-new",
         "changes": {
           "is_requestable": {
             "added": "0",
             "removed": "1"
           },
           "name": {
             "removed": "feedback",
             "added": "feedback-new"
           }
         },
         "id": 13
       }
     ]
   }

``flagtypes`` (array) Flag change objects containing the following items:

=======  ======  ================================================================
name     type    description
=======  ======  ================================================================
id       int     The ID of the flag type that was updated.
name     string  The name of the flag type that was updated.
changes  object  The changes that were actually done on this flag type.
                 The keys are the names of the fields that were changed, and the
                 values are an object with two items:

                 * added: (string) The value that this field was changed to.
                 * removed: (string) The value that was previously set in this
                   field.
=======  ======  ================================================================

Booleans changes will be represented with the strings '1' and '0'.
