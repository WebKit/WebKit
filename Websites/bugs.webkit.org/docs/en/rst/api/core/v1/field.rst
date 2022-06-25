Bug Fields
==========

The Bugzilla API for getting details about bug fields.

.. _rest_fields:

Fields
------

Get information about valid bug fields, including the lists of legal values
for each field.

**Request**

To get information about all fields:

.. code-block:: text

   GET /rest/field/bug

To get information related to a single field:

.. code-block:: text

   GET /rest/field/bug/(id_or_name)

==========  =====  ==========================================================
name        type   description
==========  =====  ==========================================================
id_or_name  mixed  An integer field ID or string representing the field name.
==========  =====  ==========================================================

**Response**

.. code-block:: js

   {
     "fields": [
       {
         "display_name": "Priority",
         "name": "priority",
         "type": 2,
         "is_mandatory": false,
         "value_field": null,
         "values": [
           {
             "sortkey": 100,
             "sort_key": 100,
             "visibility_values": [],
             "name": "P1"
           },
           {
             "sort_key": 200,
             "name": "P2",
             "visibility_values": [],
             "sortkey": 200
           },
           {
             "sort_key": 300,
             "visibility_values": [],
             "name": "P3",
             "sortkey": 300
           },
           {
             "sort_key": 400,
             "name": "P4",
             "visibility_values": [],
             "sortkey": 400
           },
           {
             "name": "P5",
             "visibility_values": [],
             "sort_key": 500,
             "sortkey": 500
           }
         ],
         "visibility_values": [],
         "visibility_field": null,
         "is_on_bug_entry": false,
         "is_custom": false,
         "id": 13
       }
     ]
   }

``field`` (array) Field objects each containing the following items:

=================  =======  =====================================================
name               type     description
=================  =======  =====================================================
id                 int      An integer ID uniquely identifying this field in this
                            installation only.
type               int      The number of the fieldtype. The following values are
                            defined:

                            * ``0`` Field type unknown
                            * ``1`` Single-line string field
                            * ``2`` Single value field
                            * ``3`` Multiple value field
                            * ``4`` Multi-line text value
                            * ``5`` Date field with time
                            * ``6`` Bug ID field
                            * ``7`` See Also field
                            * ``8`` Keywords field
                            * ``9`` Date field
                            * ``10`` Integer field

is_custom          boolean  ``true`` when this is a custom field, ``false``
                            otherwise.
name               string   The internal name of this field. This is a unique
                            identifier for this field. If this is not a custom
                            field, then this name will be the same across all
                            Bugzilla installations.
display_name       string   The name of the field, as it is shown in the user
                            interface.
is_mandatory       boolean  ``true`` if the field must have a value when filing
                            new bugs. Also, mandatory fields cannot have their
                            value cleared when updating bugs.
is_on_bug_entry    boolean  For custom fields, this is ``true`` if the field is
                            shown when you enter a new bug. For standard fields,
                            this is currently always ``false``, even if the field
                            shows up when entering a bug. (To know whether or not
                            a standard field is valid on bug entry, see
                            :ref:`rest_create_bug`.
visibility_field   string   The name of a field that controls the visibility of
                            this field in the user interface. This field only
                            appears in the user interface when the named field is
                            equal to one of the values is ``visibility_values``.
                            Can be null.
visibility_values  array    This field is only shown when ``visibility_field``
                            matches one of these string values. When
                            ``visibility_field`` is null, then this is an empty
                            array.
value_field        string   The name of the field that controls whether or not
                            particular values of the field are shown in the user
                            interface. Can be null.
values             array    Objects representing the legal values for
                            select-type (drop-down and multiple-selection)
                            fields. This is  also populated for the
                            ``component``, ``version``, ``target_milestone``,
                            and ``keywords`` fields, but not for the ``product``
                            field (you must use ``get_accessible_products`` for
                            that). For fields that aren't select-type fields,
                            this will simply be an empty array. Each object
                            contains the items described in the Value object
                            below.
=================  =======  =====================================================

Value object:

=================  =======  =====================================================
name               type     description
=================  =======  =====================================================
name               string   The actual value--this is what you would specify for
                            this field in ``create``, etc.
sort_key           int      Values, when displayed in a list, are sorted first by
                            this integer and then secondly by their name.
visibility_values  array    If ``value_field`` is defined for this field, then
                            this value is only shown if the ``value_field`` is
                            set to one of the values listed in this array. Note
                            that for per-product fields, ``value_field`` is set
                            to ``product`` and ``visibility_values`` will reflect
                            which product(s) this value appears in.
is_active          boolean  This value is defined only for certain
                            product-specific fields such as version,
                            target_milestone or component. When true, the value
                            is active; otherwise the value is not active.
description        string   The description of the value. This item is only
                            included for the ``keywords`` field.
is_open            boolean  For ``bug_status`` values, determines whether this
                            status specifies that the bug is "open" (``true``)
                            or "closed" (``false``). This item is only included
                            for the ``bug_status`` field.
can_change_to      array    For ``bug_status`` values, this is an array of
                            objects that determine which statuses you can
                            transition to from this status. (This item is only
                            included for the ``bug_status`` field.)

                            Each object contains the following items:

                            * name: (string) The name of the new status
                            * comment_required: (boolean) ``true`` if a comment
                              is required when you change a bug into this status
                              using this transition.
=================  =======  =====================================================

.. _rest_legal_values:

Legal Values
------------

**DEPRECATED** Use ''Fields'' instead.

Tells you what values are allowed for a particular field.

**Request**

To get information on the values for a field based on field name:

.. code-block:: text

   GET /rest/field/bug/(field)/values

To get information based on field name and a specific product:

.. code-block:: text

   GET /rest/field/bug/(field)/(product_id)/values

==========  ======  =============================================================
name        type    description
==========  ======  =============================================================
field       string  The name of the field you want information about.
                    This should be the same as the name you would use in
                    :ref:`rest_create_bug`, below.
product_id  int     If you're picking a product-specific field, you have to
                    specify the ID of the product you want the values for.
==========  ======  =============================================================

**Resppnse**

.. code-block:: js

   {
     "values": [
       "P1",
       "P2",
       "P3",
       "P4",
       "P5"
     ]
   }

==========  ======  =============================================================
name        type    description
==========  ======  =============================================================
values      array   The legal values for this field. The values will be sorted
                    as they normally would be in Bugzilla.
==========  ======  =============================================================
