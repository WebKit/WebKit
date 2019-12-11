.. _extensions:

Extensions
##########

One of the best ways to customize Bugzilla is by using a Bugzilla
Extension. Extensions can modify both the code and UI of Bugzilla in a way
that can be distributed to other Bugzilla users and ported forward to future
versions of Bugzilla with minimal effort. We maintain a
`list of available extensions <https://wiki.mozilla.org/Bugzilla:Addons>`_
written by other people on our wiki. You would need to
make sure that the extension in question works with your version of Bugzilla.

Or, you can write your own extension. See the :api:`Bugzilla Extension
documentation <Bugzilla/Extension.html>`
for the core documentation on how to do that. It would make sense to read
the section on :ref:`templates`. There is also a sample extension in
:file:`$BUGZILLA_HOME/extensions/Example/` which gives examples of how to
use all the code hooks.

This section explains how to achieve some common tasks using the Extension APIs.

Adding A New Page to Bugzilla
=============================

There are occasions where it's useful to add a new page to Bugzilla which
has little or no relation to other pages, and perhaps doesn't use very much
Bugzilla data. A help page, or a custom report for example. The best mechanism
for this is to use :file:`page.cgi` and the ``page_before_template`` hook.

Altering Data On An Existing Page
=================================

The ``template_before_process`` hook can be used to tweak the data displayed
on a particular existing page, if you know what template is used. It has
access to all the template variables before they are passed to the templating
engine.

Adding New Fields To Bugs
=========================

To add new fields to a bug, you need to do the following:

* Add an ``install_update_db`` hook to add the fields by calling
  ``Bugzilla::Field->create`` (only if the field doesn't already exist).
  Here's what it might look like for a single field:

  .. code-block:: perl

    my $field = new Bugzilla::Field({ name => $name });
    return if $field;
 
    $field = Bugzilla::Field->create({
        name        => $name,
        description => $description,
        type        => $type,        # From list in Constants.pm
        enter_bug   => 0,
        buglist     => 0,
        custom      => 1,
    });

* Push the name of the field onto the relevant arrays in the ``bug_columns``
  and ``bug_fields`` hooks.

* If you want direct accessors, or other functions on the object, you need to
  add a BEGIN block to your Extension.pm:

  .. code-block:: perl

    BEGIN { 
       *Bugzilla::Bug::is_foopy = \&_bug_is_foopy; 
    }
 
    ...
 
    sub _bug_is_foopy {
        return $_[0]->{'is_foopy'};
    }

* You don't have to change ``Bugzilla/DB/Schema.pm``.

* You can use ``bug_end_of_create``, ``bug_end_of_create_validators``, and
  ``bug_end_of_update`` to create or update the values for your new field.

Adding New Fields To Other Things
=================================

If you are adding the new fields to an object other than a bug, you need to
go a bit lower-level. With reference to the instructions above:

* In ``install_update_db``, use ``bz_add_column`` instead

* Push on the columns in ``object_columns`` and ``object_update_columns``
  instead of ``bug_columns``.

* Add validators for the values in ``object_validators``

The process for adding accessor functions is the same.

You can use the hooks ``object_end_of_create``,
``object_end_of_create_validators``, ``object_end_of_set_all``, and
``object_end_of_update`` to create or update the values for the new object
fields you have added. In the hooks you can check the object type being
operated on and skip any objects you don't care about. For example, if you
added a new field to the ``products`` table:

.. code-block:: perl

    sub object_end_of_create {
        my ($self, $args) = @_;
        my $class = $args->{'class'};
        my $object = $args->{'object'};
        if ($class->isa('Bugzilla::Product') {
            [...]
        }
    }

You will need to do this filtering for most of the hooks whose names begin with
``object_``.

Adding Admin Configuration Panels
=================================

If you add new functionality to Bugzilla, it may well have configurable
options or parameters. The way to allow an administrator to set those
is to add a new configuration panel.

As well as using the ``config_add_panels`` hook, you will need a template to
define the UI strings for the panel. See the templates in
:file:`template/en/default/admin/params` for examples, and put your own
template in :file:`template/en/default/admin/params` in your extension's
directory.

You can access param values from Templates using::

    [% Param('param_name') %]

and from code using:

.. code-block:: perl

    Bugzilla->params->{'param_name'}

Adding User Preferences
=======================

To add a new user preference:

* Call ``add_setting('setting_name', ['some_option', 'another_option'],
  'some_option')`` in the ``install_before_final_checks`` hook. (The last
  parameter is the name of the option which should be the default.)

* Add descriptions for the identifiers for your setting and choices
  (setting_name, some_option etc.) to the hash defined in
  :file:`global/setting-descs.none.tmpl`. Do this in a template hook:
  :file:`hook/global/setting-descs-settings.none.tmpl`. Your code can see the
  hash variable; just set more members in it.

* To change behaviour based on the setting, reference it in templates using
  ``[% user.settings.setting_name.value %]``. Reference it in code using
  ``$user->settings->{'setting_name'}->{'value'}``. The value will be one of
  the option tag names (e.g. some_option).

.. _who-can-change-what:

Altering Who Can Change What
============================

Companies often have rules about which employees, or classes of employees,
are allowed to change certain things in the bug system. For example,
only the bug's designated QA Contact may be allowed to VERIFY the bug.
Bugzilla has been
designed to make it easy for you to write your own custom rules to define
who is allowed to make what sorts of value transition.

By default, assignees, QA owners and users
with *editbugs* privileges can edit all fields of bugs,
except group restrictions (unless they are members of the groups they
are trying to change). Bug reporters also have the ability to edit some
fields, but in a more restrictive manner. Other users, without
*editbugs* privileges, cannot edit
bugs, except to comment and add themselves to the CC list.

Because this kind of change is such a common request, we have added a
specific hook for it that :ref:`extensions` can call. It's called
``bug_check_can_change_field``, and it's documented :api:`in the Hooks
documentation <Bugzilla/Hook.html#bug_check_can_change_field>`.

Checking Syntax
===============

It's not immediately obvious how to check the syntax of your extension's
Perl modules, if it contains any. Running :command:`checksetup.pl` might do
some of it, but the errors aren't necessarily massively informative.

:command:`perl -Mlib=lib -MBugzilla -e 'BEGIN { Bugzilla->extensions; } use Bugzilla::Extension::ExtensionName::Class;'`

(run from ``$BUGZILLA_HOME``) is what you need.

