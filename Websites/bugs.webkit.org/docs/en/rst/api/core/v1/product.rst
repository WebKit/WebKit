Products
========

This part of the Bugzilla API allows you to list the available products and
get information about them.

.. _rest_product_list:

List Products
-------------

Returns a list of the IDs of the products the user can search on.

**Request**

To get a list of product IDs a user can select such as for querying bugs:

.. code-block:: text

   GET /rest/product_selectable

To get a list of product IDs a user can enter a bug against:

.. code-block:: text

   GET /rest/product_enterable

To get a list of product IDs a user can search or enter bugs against.

.. code-block:: text

   GET /rest/product_accessible

**Response**

.. code-block:: js

   {
     "ids": [
       "2",
       "3",
       "19",
       "1",
       "4"
     ]
   }

====  =====  ======================================
name  type   description
====  =====  ======================================
ids   array  List of integer product IDs.
====  =====  ======================================

.. _rest_product_get:

Get Product
-----------

Returns a list of information about the products passed to it.

**Request**

To return information about a specific type of products such as
``accessible``, ``selectable``, or ``enterable``:

.. code-block:: text

   GET /rest/product?type=accessible

To return information about a specific product by ``id`` or ``name``:

.. code-block:: text

   GET /rest/product/(id_or_name)

You can also return information about more than one product by using the
following parameters in your query string:

.. code-block:: text

   GET /rest/product?ids=1&ids=2&ids=3
   GET /rest/product?names=ProductOne&names=Product2

==========  ======  =============================================================
name        type    description
==========  ======  =============================================================
id_or_name  mixed   Integer product ID or product name.
ids         array   Product IDs
names       array   Product names
type        string  The group of products to return. Valid values are
                    ``accessible`` (default), ``selectable``, and ``enterable``.
                    ``type`` can be a single value or an array of values if more
                    than one group is needed with duplicates removed.
==========  ======  =============================================================

**Response**

.. code-block:: js

   {
     "products": [
       {
         "id": 1,
         "default_milestone": "---",
         "components": [
           {
             "is_active": true,
             "default_assigned_to": "admin@bugzilla.org",
             "id": 1,
             "sort_key": 0,
             "name": "TestComponent",
             "flag_types": {
               "bug": [
                 {
                   "is_active": true,
                   "grant_group": null,
                   "cc_list": "",
                   "is_requestable": true,
                   "id": 3,
                   "is_multiplicable": true,
                   "name": "needinfo",
                   "request_group": null,
                   "is_requesteeble": true,
                   "sort_key": 0,
                   "description": "needinfo"
                 }
               ],
               "attachment": [
                 {
                   "description": "Review",
                   "is_multiplicable": true,
                   "name": "review",
                   "is_requesteeble": true,
                   "request_group": null,
                   "sort_key": 0,
                   "cc_list": "",
                   "grant_group": null,
                   "is_requestable": true,
                   "id": 2,
                   "is_active": true
                 }
               ]
             },
             "default_qa_contact": "",
             "description": "This is a test component."
           }
         ],
         "is_active": true,
         "classification": "Unclassified",
         "versions": [
           {
             "id": 1,
             "name": "unspecified",
             "is_active": true,
             "sort_key": 0
           }
         ],
         "description": "This is a test product.",
         "has_unconfirmed": true,
         "milestones": [
           {
             "name": "---",
             "is_active": true,
             "sort_key": 0,
             "id": 1
           }
         ],
         "name": "TestProduct"
       }
     ]
   }

``products`` (array) Each product object has the following items:

=================  =======  =====================================================
name               type     description
=================  =======  =====================================================
id                 int      An integer ID uniquely identifying the product in
                            this installation only.
name               string   The name of the product. This is a unique identifier
                            for the product.
description        string   A description of the product, which may contain HTML.
is_active          boolean  A boolean indicating if the product is active.
default_milestone  string   The name of the default milestone for the product.
has_unconfirmed    boolean  Indicates whether the UNCONFIRMED bug status is
                            available for this product.
classification     string   The classification name for the product.
components         array    Each component object has the items described in the
                            Component object below.
versions           array    Each object describes a version, and has the
                            following items: ``name``, ``sort_key`` and
                            ``is_active``.
milestones         array    Each object describes a milestone, and has the
                            following items: ``name``, ``sort_key`` and
                            ``is_active``.
=================  =======  =====================================================

If the user tries to access a product that is not in the list of accessible
products for the user, or a product that does not exist, that is silently
ignored, and no information about that product is returned.

Component object:

===================  =======  ===================================================
name                 type     description
===================  =======  ===================================================
id                   int      An integer ID uniquely identifying the component in
                              this installation only.
name                 string   The name of the component.  This is a unique
                              identifier for this component.
description          string   A description of the component, which may contain
                              HTML.
default_assigned_to  string   The login name of the user to whom new bugs
                              will be assigned by default.
default_qa_contact   string   The login name of the user who will be set as
                              the QA Contact for new bugs by default. Empty
                              string if the QA contact is not defined.
sort_key             int      Components, when displayed in a list, are sorted
                              first by this integer and then secondly by their
                              name.
is_active            boolean  A boolean indicating if the component is active.
                              Inactive components are not enabled for new bugs.
flag_types           object   An object containing two items ``bug`` and
                              ``attachment`` that each contains an array of
                              objects, where each describes a flagtype. The
                              flagtype items are described in the Flagtype
                              object below.
===================  =======  ===================================================

Flagtype object:

================  =======  ======================================================
name              type     description
================  =======  ======================================================
id                int      Returns the ID of the flagtype.
name              string   Returns the name of the flagtype.
description       string   Returns the description of the flagtype.
cc_list           string   Returns the concatenated CC list for the flagtype, as
                           a single string.
sort_key          int      Returns the sortkey of the flagtype.
is_active         boolean  Returns whether the flagtype is active or disabled.
                           Flags being in a disabled flagtype are not deleted.
                           It only prevents you from adding new flags to it.
is_requestable    boolean  Returns whether you can request for the given
                           flagtype (i.e. whether the '?' flag is available or
                           not).
is_requesteeble   boolean  Returns whether you can ask someone specifically
                           or not.
is_multiplicable  boolean  Returns whether you can have more than one
                           flag for the given flagtype in a given bug/attachment.
grant_group       int      the group ID that is allowed to grant/deny flags of
                           this type. If the item is not included all users are
                           allowed to grant/deny this flagtype.
request_group     int      The group ID that is allowed to request the flag if
                           the flag is of the type requestable. If the item is
                           not included all users are allowed request this
                           flagtype.
================  =======  ======================================================

.. _rest_product_create:

Create Product
--------------

This allows you to create a new product in Bugzilla.

**Request**

.. code-block:: text

   POST /rest/product

.. code-block:: js

   {
     "name" : "AnotherProduct",
     "description" : "Another Product",
     "classification" : "Unclassified",
     "is_open" : false,
     "has_unconfirmed" : false,
     "version" : "unspecified"
   }

Some params must be set, or an error will be thrown. The required params are
marked in bold.

=================  =======  =====================================================
name               type     description
=================  =======  =====================================================
**name**           string   The name of this product. Must be globally unique
                            within Bugzilla.
**description**    string   A description for this product. Allows some simple
                            HTML.
**version**        string   The default version for this product.
has_unconfirmed    boolean  Allow the UNCONFIRMED status to be set on bugs in
                            this product. Default: true.
classification     string   The name of the Classification which contains this
                            product.
default_milestone  string   The default milestone for this product. Default
                            '---'.
is_open            boolean  ``true`` if the product is currently allowing bugs
                            to be entered into it. Default: ``true``.
create_series      boolean  ``true`` if you want series for New Charts to be
                            created for this new product. Default: ``true``.
=================  =======  =====================================================

**Response**

.. code-block:: js

   {
     "id": 20
   }

Returns an object with the following items:

====  ====  =====================================
name  type  description
====  ====  =====================================
id    int   ID of the newly-filed product.
====  ====  =====================================

.. _rest_product_update:

Update Product
--------------

This allows you to update a product in Bugzilla.

**Request**

.. code-block:: text

   PUT /rest/product/(id_or_name)

You can edit a single product by passing the ID or name of the product
in the URL. To edit more than one product, you can specify addition IDs or
product names using the ``ids`` or ``names`` parameters respectively.

.. code-block:: js

   {
     "ids" : [123],
     "name" : "BarName",
     "has_unconfirmed" : false
   }

One of the below must be specified.

==============  =====  ==========================================================
name            type   description
==============  =====  ==========================================================
**id_or_name**  mixed  Integer product ID or name.
**ids**         array  Numeric IDs of the products that you wish to update.
**names**       array  Names of the products that you wish to update.
==============  =====  ==========================================================

The following parameters specify the new values you want to set for the product(s)
you are updating.

=================  =======  =====================================================
name               type     description
=================  =======  =====================================================
name               string   A new name for this product. If you try to set this
                            while updating more than one product, an error will
                            occur, as product names must be unique.
default_milestone  string   When a new bug is filed, what milestone does it
                            get by default if the user does not choose one? Must
                            represent a milestone that is valid for this product.
description        string   Update the long description for these products to
                            this value.
has_unconfirmed    boolean  Allow the UNCONFIRMED status to be set on bugs in
                            products.
is_open            boolean  ``true`` if the product is currently allowing bugs
                            to be entered into it, ``false`` otherwise.
=================  =======  =====================================================

**Response**

.. code-block:: js

   {
      "products" : [
         {
            "id" : 123,
            "changes" : {
               "name" : {
                  "removed" : "FooName",
                  "added" : "BarName"
               },
               "has_unconfirmed" : {
                  "removed" : "1",
                  "added" : "0"
               }
            }
         }
      ]
   }

``products`` (array) Product change objects containing the following items:

=======  ======  ================================================================
name     type    description
=======  ======  ================================================================
id       int     The ID of the product that was updated.
changes  object  The changes that were actually done on this product. The
                 keys are the names of the fields that were changed, and the
                 values are an object with two items:

                 * added: (string) The value that this field was changed to.
                 * removed: (string) The value that was previously set in this
                   field.
=======  ======  ================================================================

Booleans will be represented with the strings '1' and '0' for changed values
as they are stored as strings in the database currently.
