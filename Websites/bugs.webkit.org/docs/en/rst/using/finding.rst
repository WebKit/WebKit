.. _finding:

Finding Bugs
############

Bugzilla has a number of different search options.

.. note:: Bugzilla queries are case-insensitive and accent-insensitive when
    used with either MySQL or Oracle databases. When using Bugzilla with
    PostgreSQL, however, some queries are case sensitive. This is due to
    the way PostgreSQL handles case and accent sensitivity.

.. _quicksearch:

Quicksearch
===========

Quicksearch is a single-text-box query tool. You'll find it in
Bugzilla's header or footer.

Quicksearch uses
metacharacters to indicate what is to be searched. For example, typing

  ``foo|bar``

into Quicksearch would search for "foo" or "bar" in the
summary and status whiteboard of a bug; adding

  ``:BazProduct``

would search only in that product.

You can also use it to go directly to a bug by entering its number or its
alias.

Simple Search
=============

Simple Search is good for finding one particular bug. It works like internet
search engines - just enter some keywords and off you go.

Advanced Search
===============

The Advanced Search page is used to produce a list of all bugs fitting
exact criteria. `You can play with it on
Landfill <http://landfill.bugzilla.org/bugzilla-tip/query.cgi?format=advanced>`_.

Advanced Search has controls for selecting different possible
values for all of the fields in a bug, as described above. For some
fields, multiple values can be selected. In those cases, Bugzilla
returns bugs where the content of the field matches any one of the selected
values. If none is selected, then the field can take any value.

After a search is run, you can save it as a Saved Search, which
will appear in the page footer. If you are in the group defined
by the "querysharegroup" parameter, you may share your queries
with other users; see :ref:`saved-searches` for more details.

.. _custom-search:

Custom Search
=============

Highly advanced querying is done using the :guilabel:`Custom Search` feature
of the :guilabel:`Advanced Search` page.

The search criteria here further restrict the set of results
returned by a query, over and above those defined in the fields at the top
of the page. It is thereby possible to search for bugs
based on elaborate combinations of criteria.

The simplest custom searches have only one term. These searches
permit the selected *field*
to be compared using a
selectable *operator* to a
specified *value.* Much of this could be reproduced using the standard
fields. However, you can then combine terms using "Match ANY" or "Match ALL",
using parentheses for combining and priority, in order to construct searches
of almost arbitrary complexity.

There are three fields in each row (known as a "term") of a custom search:

- *Field:*
  the name of the field being searched

- *Operator:*
  the comparison operator

- *Value:*
  the value to which the field is being compared

The list of available *fields* contains all the fields defined for a bug,
including any custom fields, and then also some pseudofields like
:guilabel:`Assignee Real Name`, :guilabel:`Days Since Bug Changed`,
:guilabel:`Time Since Assignee Touched` and other things it may be useful to
search on.

There are a wide range of *operators* available, not all of which may make
sense for a particular field. There are various string-matching operations
(including regular expressions), numerical comparisons (which also work for
dates), and also the ability to search for change information—when a field
changed, what it changed from or to, and who did it. There are special
operators for :guilabel:`is empty` and :guilabel:`is not empty`, because
Bugzilla can't tell the difference between a value field left blank on
purpose and one left blank by accident.

You can have an arbitrary number of rows, and the dropdown box above them
defines how they relate—:guilabel:`Match ALL of the following separately`,
:guilabel:`Match ANY of the following separately`, or :guilabel:`Match ALL of
the following against the same field`. The difference between the first and
the third can be illustrated with a comment search. If you have a search::

    Comment   contains the string   "Fred"
    Comment   contains the string   "Barney"

then under the first regime (match separately) the search would return bugs
where "Fred" appeared in one comment and "Barney" in the same or any other
comment, whereas under the second (match against the same field), both strings
would need to occur in exactly the same comment.

.. _advanced-features:

Advanced Features
-----------------

If you click :guilabel:`Show Advanced Features`, then more capabilities appear.
You can negate any row with a checkbox (see below) and also group lines of the
search with parentheses to determine how different search terms relate. Within
each bracketed set, you get the choice of combining them using ALL (i.e. AND)
or ANY (i.e. OR).

Negation
--------

At first glance, negation seems redundant. Rather than
searching for::

    NOT ( summary   contains the string   "foo" )

one could search for::

    summary   does not contain the string   "foo"

However, the search::

    CC   does not contain the string   "@mozilla.org"

would find every bug where anyone on the CC list did not contain
"@mozilla.org" while::

    NOT ( CC   contains the string   "@mozilla.org" )

would find every bug where there was nobody on the CC list who
did contain the string. Similarly, the use of negation also permits
complex expressions to be built using terms OR'd together and then
negated. Negation permits queries such as::

    NOT ( ( product   equals   "Update" )
          OR
          ( component   equals   "Documentation" )
        )

to find bugs that are neither
in the :guilabel:`Update` product or in the :guilabel:`Documentation` component
or::

    NOT ( ( commenter   equals   "%assignee%" )
          OR
          (component   equals   "Documentation" )
        )

to find non-documentation bugs on which the assignee has never commented.

.. _pronouns:

Pronoun Substitution
--------------------

Sometimes, a query needs to compare a user-related field
(such as :guilabel:`Reporter`) with a role-specific user (such as the
user running the query or the user to whom each bug is assigned). For
example, you may want to find all bugs that are assigned to the person
who reported them.

When the :guilabel:`Custom Search` operator is either :guilabel:`equals` or
:guilabel:`notequals`, the value can be "%reporter%", "%assignee%",
"%qacontact%", or "%user%". These are known as "pronouns". The user pronoun
refers to the user who is executing the query or, in the case
of whining reports, the user who will be the recipient
of the report. The reporter, assignee, and qacontact
pronouns refer to the corresponding fields in the bug.

This feature also lets you search by a user's group memberships. If the
operator is either :guilabel:`equals`, :guilabel:`notequals` or
:guilabel:`anyexact`, you can search for
whether a user belongs (or not) to the specified group. The group name must be
entered using "%group.foo%" syntax, where "foo" is the group name.
So if you are looking for bugs reported by any user being in the
"editbugs" group, then you can use::

    reporter   equals   "%group.editbugs%"

.. _list:

Bug Lists
=========

The result of a search is a list of matching bugs.

The format of the list is configurable. For example, it can be
sorted by clicking the column headings. Other useful features can be
accessed using the links at the bottom of the list:

Long Format:
    this gives you a large page with a non-editable summary of the fields
    of each bug.

XML (icon):
    get the buglist in an XML format.

CSV (icon):
    get the buglist as comma-separated values, for import into e.g.
    a spreadsheet.

Feed (icon):
    get the buglist as an Atom feed.  Copy this link into your
    favorite feed reader.  If you are using Firefox, you can also
    save the list as a live bookmark by clicking the live bookmark
    icon in the status bar.  To limit the number of bugs in the feed,
    add a limit=n parameter to the URL.

iCalendar (icon):
    Get the buglist as an iCalendar file. Each bug is represented as a
    to-do item in the imported calendar.

Change Columns:
    change the bug attributes which appear in the list.

Change Several Bugs At Once:
    If your account is sufficiently empowered, and more than one bug
    appears in the bug list, this link is displayed and lets you easily make
    the same change to all the bugs in the list - for example, changing
    their assignee.

Send Mail to Bug Assignees:
    If more than one bug appear in the bug list and there are at least
    two distinct bug assignees, this links is displayed which lets you
    easily send a mail to the assignees of all bugs on the list.

Edit Search:
    If you didn't get exactly the results you were looking for, you can
    return to the Query page through this link and make small revisions
    to the query you just made so you get more accurate results.

Remember Search As:
    You can give a search a name and remember it; a link will appear
    in your page footer giving you quick access to run it again later.

