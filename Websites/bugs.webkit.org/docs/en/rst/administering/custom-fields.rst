.. _custom-fields:

Custom Fields
#############

Custom Fields are fields defined by the administrator, in addition to those
which come with Bugzilla by default. Custom Fields are treated like any other
field—they can be set in bugs and used for search queries.

Administrators should keep in mind that
adding too many fields can make the user interface more complicated and
harder to use. Custom Fields should be added only when necessary and with
careful consideration.

.. note:: Before adding a Custom Field, make sure that Bugzilla cannot already
   do the desired behavior. Many Bugzilla options are not enabled by
   default, and many times Administrators find that simply enabling
   certain options that already exist is sufficient.

Administrators can manage Custom Fields using the
``Custom Fields`` link on the Administration page. The Custom
Fields administration page displays a list of Custom Fields, if any exist,
and a link to "Add a new custom field".

.. _add-custom-fields:

Adding Custom Fields
====================

To add a new Custom Field, click the "Add a new custom field" link. This
page displays several options for the new field, described below.

The following attributes must be set for each new custom field:

- *Name:*
  The name of the field in the database, used internally. This name
  MUST begin with ``cf_`` to prevent confusion with
  standard fields. If this string is omitted, it will
  be automatically added to the name entered.

- *Description:*
  A brief string used as the label for this Custom Field.
  That is the string that users will see, and it should be
  short and explicit.

- *Type:*
  The type of field to create. There are
  several types available:

  Bug ID:
      A field where you can enter the ID of another bug from
      the same Bugzilla installation. To point to a bug in a remote
      installation, use the See Also field instead.
  Large Text Box:
      A multiple line box for entering free text.
  Free Text:
      A single line box for entering free text.
  Multiple-Selection Box:
      A list box where multiple options
      can be selected. After creating this field, it must be edited
      to add the selection options. See
      :ref:`edit-values-list` for information about
      editing legal values.
  Drop Down:
      A list box where only one option can be selected.
      After creating this field, it must be edited to add the
      selection options. See
      :ref:`edit-values-list` for information about
      editing legal values.
  Date/Time:
      A date field. This field appears with a
      calendar widget for choosing the date.

- *Sortkey:*
  Integer that determines in which order Custom Fields are
  displayed in the User Interface, especially when viewing a bug.
  Fields with lower values are displayed first.

- *Reverse Relationship Description:*
  When the custom field is of type ``Bug ID``, you can
  enter text here which will be used as label in the referenced
  bug to list bugs which point to it. This gives you the ability
  to have a mutual relationship between two bugs.

- *Can be set on bug creation:*
  Boolean that determines whether this field can be set on
  bug creation. If not selected, then a bug must be created
  before this field can be set. See :ref:`filing`
  for information about filing bugs.

- *Displayed in bugmail for new bugs:*
  Boolean that determines whether the value set on this field
  should appear in bugmail when the bug is filed. This attribute
  has no effect if the field cannot be set on bug creation.

- *Is obsolete:*
  Boolean that determines whether this field should
  be displayed at all. Obsolete Custom Fields are hidden.

- *Is mandatory:*
  Boolean that determines whether this field must be set.
  For single and multi-select fields, this means that a (non-default)
  value must be selected; for text and date fields, some text
  must be entered.

- *Field only appears when:*
  A custom field can be made visible when some criteria is met.
  For instance, when the bug belongs to one or more products,
  or when the bug is of some given severity. If left empty, then
  the custom field will always be visible, in all bugs.

- *Field that controls the values that appear in this field:*
  When the custom field is of type ``Drop Down`` or
  ``Multiple-Selection Box``, you can restrict the
  availability of the values of the custom field based on the
  value of another field. This criteria is independent of the
  criteria used in the ``Field only appears when``
  setting. For instance, you may decide that some given value
  ``valueY`` is only available when the bug status
  is RESOLVED while the value ``valueX`` should
  always be listed.
  Once you have selected the field that should control the
  availability of the values of this custom field, you can
  edit values of this custom field to set the criteria; see
  :ref:`edit-values-list`.

.. _edit-custom-fields:

Editing Custom Fields
=====================

As soon as a Custom Field is created, its name and type cannot be
changed. If this field is a drop-down menu, its legal values can
be set as described in :ref:`edit-values-list`. All
other attributes can be edited as described above.

.. _delete-custom-fields:

Deleting Custom Fields
======================

Only custom fields that are marked as obsolete, and that have never
been used, can be deleted completely (else the integrity
of the bug history would be compromised). For custom fields marked
as obsolete, a "Delete" link will appear in the ``Action``
column. If the custom field has been used in the past, the deletion
will be rejected. Marking the field as obsolete, however, is sufficient
to hide it from the user interface entirely.

