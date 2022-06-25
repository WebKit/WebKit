.. _field-values:

Field Values
############

Legal values for the operating system, platform, bug priority and
severity, and custom fields of type ``Drop Down`` and
``Multiple-Selection Box`` (see :ref:`custom-fields`),
as well as the list of valid bug statuses and resolutions, can be
customized from the same interface. You can add, edit, disable, and
remove the values that can be used with these fields.

.. _edit-values-list:

Viewing/Editing Legal Values
============================

Editing legal values requires ``admin`` privileges.
Select "Field Values" from the Administration page. A list of all
fields, both system and Custom, for which legal values
can be edited appears. Click a field name to edit its legal values.

There is no limit to how many values a field can have, but each value
must be unique to that field. The sortkey is important to display these
values in the desired order.

When the availability of the values of a custom field is controlled
by another field, you can select from here which value of the other field
must be set for the value of the custom field to appear.

.. _edit-values-delete:

Deleting Legal Values
=====================

Legal values from Custom Fields can be deleted, but only if the
following two conditions are respected:

#. The value is not set as the default for the field.

#. No bug is currently using this value.

If any of these conditions is not respected, the value cannot be deleted.
The only way to delete these values is to reassign bugs to another value
and to set another value as default for the field.
