
.. _customization-faq:

Customization FAQ
=================

How do I...

...add a new field on a bug?
  Use :ref:`custom-fields` or, if you just want new form fields on bug entry
  but don't need Bugzilla to track the field seperately thereafter, you can
  use a :ref:`custom bug entry form <custom-bug-entry>`.

...change the name of a built-in bug field?
  :ref:`Edit <templates>` the relevant value in the template
  :file:`template/en/default/global/field-descs.none.tmpl`.

...use a word other than 'bug' to describe bugs?
  :ref:`Edit or override <templates>` the appropriate values in the template
  :file:`template/en/default/global/variables.none.tmpl`.
  
...call the system something other than 'Bugzilla'?
  :ref:`Edit or override <templates>` the appropriate value in the template
  :file:`template/en/default/global/variables.none.tmpl`.
  
...alter who can change what field when?
  See :ref:`who-can-change-what`.
