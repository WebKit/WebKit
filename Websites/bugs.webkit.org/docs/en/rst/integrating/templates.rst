.. _templates:

Templates
#########

Bugzilla uses a system of templates to define its user interface. The standard
templates can be modified, replaced or overridden. You can also use template
hooks in an :ref:`extension <extensions>` to add or modify the
behaviour of templates using a stable interface.

.. _template-directory:

Template Directory Structure
============================

The template directory structure starts with top level directory
named :file:`template`, which contains a directory
for each installed localization. Bugzilla comes with English
templates, so the directory name is :file:`en`,
and we will discuss :file:`template/en` throughout
the documentation. Below :file:`template/en` is the
:file:`default` directory, which contains all the
standard templates shipped with Bugzilla.

.. warning:: A directory :file:`data/template` also exists;
   this is where Template Toolkit puts the compiled versions (i.e. Perl code)
   of the templates. *Do not* directly edit the files in this
   directory, or all your changes will be lost the next time
   Template Toolkit recompiles the templates.

.. _template-method:

Choosing a Customization Method
===============================

If you want to edit Bugzilla's templates, the first decision
you must make is how you want to go about doing so. There are three
choices, and which you use depends mainly on the scope of your
modifications, and the method you plan to use to upgrade Bugzilla.

#. You can directly edit the templates found in :file:`template/en/default`.

#. You can copy the templates to be modified into a mirrored directory
   structure under :file:`template/en/custom`. Templates in this
   directory structure automatically override any identically-named
   and identically-located templates in the
   :file:`template/en/default` directory. (The :file:`custom` directory does
   not exist by default and must be created if you want to use it.)

#. You can use the hooks built into many of the templates to add or modify
   the UI from an :ref:`extension <extensions>`. Hooks generally don't go away
   and have a stable interface. 

The third method is the best if there are hooks in the appropriate places
and the change you want to do is possible using hooks. It's not very easy
to modify existing UI using hooks; they are most commonly used for additions.
You can make modifications if you add JS code which then makes the
modifications when the page is loaded. You can remove UI by adding CSS to hide
it.

Unlike code hooks, there is no requirement to document template hooks, so
you just have to open up the template and see (search for ``Hook.process``).

If there are no hooks available, then the second method of customization
should be used if you are going to make major changes, because it is
guaranteed that the contents of the :file:`custom` directory will not be
touched during an upgrade, and you can then decide whether
to revert to the standard templates, continue using yours, or make the effort
to merge your changes into the new versions by hand. It's also good for
entirely new files, and for a few files like
:file:`bug/create/user-message.html.tmpl` which are designed to be entirely
replaced.

Using the second method, your user interface may break if incompatible
changes are made to the template interface. Templates do change regularly
and so interface changes are not individually documented, and you would
need to work out what had changed and adapt your template accordingly.

For minor changes, the convenience of the first method is hard to beat. When
you upgrade Bugzilla, :command:`git` will merge your changes into the new
version for you. On the downside, if the merge fails then Bugzilla will not
work properly until you have fixed the problem and re-integrated your code.

Also, you can see what you've changed using :command:`git diff`, which you
can't if you fork the file into the :file:`custom` directory.

.. _template-edit:

How To Edit Templates
=====================

.. note:: If you are making template changes that you intend on submitting 
   back for inclusion in standard Bugzilla, you should read the relevant
   sections of the
   `Developers' Guide <http://www.bugzilla.org/docs/developer.html>`_.

Bugzilla uses a templating system called Template Toolkit. The syntax of the
language is beyond the scope of this guide. It's reasonably easy to pick up by
looking at the current templates; or, you can read the manual, available on
the `Template Toolkit home page <http://www.template-toolkit.org>`_.

One thing you should take particular care about is the need
to properly HTML filter data that has been passed into the template.
This means that if the data can possibly contain special HTML characters
such as ``<``, and the data was not intended to be HTML, they need to be
converted to entity form, i.e. ``&lt;``.  You use the ``html`` filter in the
Template Toolkit to do this (or the ``uri`` filter to encode special
characters in URLs).  If you forget, you may open up your installation
to cross-site scripting attacks.


You should run :command:`./checksetup.pl` after editing any templates. Failure
to do so may mean either that your changes are not picked up, or that the
permissions on the edited files are wrong so the webserver can't read them. 

.. _template-formats:

Template Formats and Types
==========================

Some CGI's have the ability to use more than one template. For example,
:file:`buglist.cgi` can output itself as two formats of HTML (complex and
simple). Each of these is a separate template. The mechanism that provides
this feature is extensible - you can create new templates to add new formats.

You might use this feature to e.g. add a custom bug entry form for a
particular subset of users or a particular type of bug.

Bugzilla can also support different types of output - e.g. bugs are available
as HTML and as XML, and this mechanism is extensible also to add new content
types. However, instead of using such interfaces or enhancing Bugzilla to add
more, you would be better off using the :ref:`apis` to integrate with
Bugzilla.

To see if a CGI supports multiple output formats and types, grep the
CGI for ``get_format``. If it's not present, adding
multiple format/type support isn't too hard - see how it's done in
other CGIs, e.g. :file:`config.cgi`.

To make a new format template for a CGI which supports this,
open a current template for
that CGI and take note of the INTERFACE comment (if present.) This
comment defines what variables are passed into this template. If
there isn't one, I'm afraid you'll have to read the template and
the code to find out what information you get.

Write your template in whatever markup or text style is appropriate.

You now need to decide what content type you want your template
served as. The content types are defined in the
:file:`Bugzilla/Constants.pm` file in the :file:`contenttypes`
constant. If your content type is not there, add it. Remember
the three- or four-letter tag assigned to your content type.
This tag will be part of the template filename.

Save your new template as
:file:`<stubname>-<formatname>.<contenttypetag>.tmpl`.
Try out the template by calling the CGI as
``<cginame>.cgi?format=<formatname>``. Add ``&ctype=<type>`` if the type is
not HTML.

.. _template-specific:

Particular Templates
====================

There are a few templates you may be particularly interested in
customizing for your installation.

:file:`index.html.tmpl`:
  This is the Bugzilla front page.

:file:`global/header.html.tmpl`:
  This defines the header that goes on all Bugzilla pages.
  The header includes the banner, which is what appears to users
  and is probably what you want to edit instead.  However the
  header also includes the HTML HEAD section, so you could for
  example add a stylesheet or META tag by editing the header.

:file:`global/banner.html.tmpl`:
  This contains the ``banner``, the part of the header that appears
  at the top of all Bugzilla pages.  The default banner is reasonably
  barren, so you'll probably want to customize this to give your
  installation a distinctive look and feel.  It is recommended you
  preserve the Bugzilla version number in some form so the version
  you are running can be determined, and users know what docs to read.

:file:`global/footer.html.tmpl`:
  This defines the footer that goes on all Bugzilla pages.  Editing
  this is another way to quickly get a distinctive look and feel for
  your Bugzilla installation.

:file:`global/variables.none.tmpl`:
  This allows you to change the word 'bug' to something else (e.g. "issue")
  throughout the interface, and also to change the name Bugzilla to something
  else (e.g. "FooCorp Bug Tracker").

:file:`list/table.html.tmpl`:
  This template controls the appearance of the bug lists created
  by Bugzilla. Editing this template allows per-column control of
  the width and title of a column, the maximum display length of
  each entry, and the wrap behaviour of long entries.
  For long bug lists, Bugzilla inserts a 'break' every 100 bugs by
  default; this behaviour is also controlled by this template, and
  that value can be modified here.

:file:`bug/create/user-message.html.tmpl`:
  This is a message that appears near the top of the bug reporting page.
  By modifying this, you can tell your users how they should report
  bugs.

:file:`bug/process/midair.html.tmpl`:
  This is the page used if two people submit simultaneous changes to the
  same bug.  The second person to submit their changes will get this page
  to tell them what the first person did, and ask if they wish to
  overwrite those changes or go back and revisit the bug.  The default
  title and header on this page read "Mid-air collision detected!"  If
  you work in the aviation industry, or other environment where this
  might be found offensive (yes, we have true stories of this happening)
  you'll want to change this to something more appropriate for your
  environment.

.. _custom-bug-entry:

:file:`bug/create/create.html.tmpl` and :file:`bug/create/comment.txt.tmpl`:
    You may not wish to go to the effort of creating custom fields in
    Bugzilla, yet you want to make sure that each bug report contains
    a number of pieces of important information for which there is not
    a special field. The bug entry system has been designed in an
    extensible fashion to enable you to add arbitrary HTML widgets,
    such as drop-down lists or textboxes, to the bug entry page
    and have their values appear formatted in the initial comment.

    An example of this is the `guided bug submission form
    <http://landfill.bugzilla.org/bugzilla-tip/enter_bug.cgi?product=WorldControl;format=guided>`_.
    The code for this comes with the Bugzilla distribution as an example for
    you to copy. It can be found in the files
    :file:`create-guided.html.tmpl` and :file:`comment-guided.html.tmpl`.
        
    A hidden field that indicates the format should be added inside
    the form in order to make the template functional. Its value should
    be the suffix of the template filename. For example, if the file
    is called :file:`create-guided.html.tmpl`, then

    ::

        <input type="hidden" name="format" value="guided">

    is used inside the form.

    So to use this feature, create a custom template for
    :file:`enter_bug.cgi`. The default template, on which you
    could base it, is
    :file:`default/bug/create/create.html.tmpl`.
    Call it :file:`custom/bug/create/create-<formatname>.html.tmpl`, and
    in it, add form inputs for each piece of information you'd like
    collected - such as a build number, or set of steps to reproduce.

    Then, create a template based on
    :file:`default/bug/create/comment.txt.tmpl`, and call it
    :file:`custom/bug/create/comment-<formatname>.txt.tmpl`.
    It needs a couple of lines of boilerplate at the top like this::

        [% USE Bugzilla %]
        [% cgi = Bugzilla.cgi %

    Then, this template can reference the form fields you have created using
    the syntax ``[% cgi.param("field_name") %]``. When a bug report is
    submitted, the initial comment attached to the bug report will be
    formatted according to the layout of this template.

    For example, if your custom enter_bug template had a field::

        <input type="text" name="buildid" size="30">

    and then your comment.txt.tmpl had::

        [% USE Bugzilla %]
        [% cgi = Bugzilla.cgi %]
        Build Identifier: [%+ cgi.param("buildid") %]

    then something like::

        Build Identifier: 20140303

    would appear in the initial comment.

    This system allows you to gather structured data in bug reports without
    the overhead and UI complexity of a large number of custom fields.
