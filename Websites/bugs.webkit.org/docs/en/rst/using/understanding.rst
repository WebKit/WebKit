.. _understanding:

Understanding a Bug
###################

The core of Bugzilla is the screen which displays a particular
bug. Note that the labels for most fields are hyperlinks;
clicking them will take you to context-sensitive help on that
particular field. Fields marked * may not be present on every
installation of Bugzilla.

*Summary:*
   A one-sentence summary of the problem, displayed in the header next to
   the bug number.

*Status (and Resolution):*
   These define exactly what state the bug is in—from not even
   being confirmed as a bug, through to being fixed and the fix
   confirmed by Quality Assurance. The different possible values for
   Status and Resolution on your installation should be documented in the
   context-sensitive help for those items.

*Alias:*
   A unique short text name for the bug, which can be used instead of the
   bug number.

*Product and Component*:
   Bugs are divided up by Product and Component, with a Product
   having one or more Components in it. 

*Version:*
   The "Version" field usually contains the numbers or names of released
   versions of the product. It is used to indicate the version(s) affected by
   the bug report.

*Hardware (Platform and OS):*
   These indicate the computing environment where the bug was
   found.

*Importance (Priority and Severity):*
   The Priority field is used to prioritize bugs, either by the assignee,
   or someone else with authority to direct their time such as a project
   manager. It's a good idea not to change this on other people's bugs. The
   default values are P1 to P5.

   The Severity field indicates how severe the problem is—from blocker
   ("application unusable") to trivial ("minor cosmetic issue"). You
   can also use this field to indicate whether a bug is an enhancement
   request.

*\*Target Milestone:*
   A future version by which the bug is to
   be fixed. e.g. The Bugzilla Project's milestones for future
   Bugzilla versions are 4.4, 5.0, 6.0, etc. Milestones are not
   restricted to numbers, though—you can use any text strings, such
   as dates.

*Assigned To:*
   The person responsible for fixing the bug.

*\*QA Contact:*
   The person responsible for quality assurance on this bug.

*URL:*
   A URL associated with the bug, if any.

*\*Whiteboard:*
   A free-form text area for adding short notes and tags to a bug.

*Keywords:*
   The administrator can define keywords which you can use to tag and
   categorise bugs—e.g. ``crash`` or ``regression``.

*Personal Tags:*
   Unlike Keywords which are global and visible by all users, Personal Tags
   are personal and can only be viewed and edited by their author. Editing
   them won't send any notifications to other users. Use them to tag and keep
   track of sets of bugs that you personally care about, using your own
   classification system.

*Dependencies (Depends On and Blocks):*
   If this bug cannot be fixed unless other bugs are fixed (depends
   on), or this bug stops other bugs being fixed (blocks), their
   numbers are recorded here.

   Clicking the :guilabel:`Dependency tree` link shows
   the dependency relationships of the bug as a tree structure.
   You can change how much depth to show, and you can hide resolved bugs
   from this page. You can also collapse/expand dependencies for
   each non-terminal bug on the tree view, using the [-]/[+] buttons that
   appear before the summary.

*Reported:*
   The person who filed the bug, and the date and time they did it.

*Modified:*
   The date and time the bug was last changed.

*CC List:*
   A list of people who get mail when the bug changes, in addition to the
   Reporter, Assignee and QA Contact (if enabled).

*Ignore Bug Mail:*
   Set this if you want never to get bugmail from this bug again. See also
   :ref:`emailpreferences`.

*\*See Also:*
   Bugs, in this Bugzilla, other Bugzillas, or other bug trackers, that are
   related to this one.

*Flags:*
   A flag is a kind of status that can be set on bugs or attachments
   to indicate that the bugs/attachments are in a certain state.
   Each installation can define its own set of flags that can be set
   on bugs or attachments. See :ref:`flags`.

*\*Time Tracking:*
   This form can be used for time tracking.
   To use this feature, you have to be a member of the group
   specified by the :param:`timetrackinggroup` parameter. See
   :ref:`time-tracking` for more information.

   Orig. Est.:
       This field shows the original estimated time.
   Current Est.:
       This field shows the current estimated time.
       This number is calculated from ``Hours Worked``
       and ``Hours Left``.
   Hours Worked:
       This field shows the number of hours worked.
   Hours Left:
       This field shows the ``Current Est.`` -
       ``Hours Worked``.
       This value + ``Hours Worked`` will become the
       new Current Est.
   %Complete:
       This field shows what percentage of the task is complete.
   Gain:
       This field shows the number of hours that the bug is ahead of the
       ``Orig. Est.``.
   Deadline:
       This field shows the deadline for this bug.

*Attachments:*
   You can attach files (e.g. test cases or patches) to bugs. If there
   are any attachments, they are listed in this section. See
   :ref:`attachments` for more information.

*Additional Comments:*
   You can add your two cents to the bug discussion here, if you have
   something worthwhile to say.

.. _flags:

Flags
=====

Flags are a way to attach a specific status to a bug or attachment,
either ``+`` or ``-``. The meaning of these symbols depends on the name of
the flag itself, but contextually they could mean pass/fail,
accept/reject, approved/denied, or even a simple yes/no. If your site
allows requestable flags, then users may set a flag to ``?`` as a
request to another user that they look at the bug/attachment and set
the flag to its correct status.

A set flag appears in bug reports and on "edit attachment" pages with the
abbreviated username of the user who set the flag prepended to the
flag name. For example, if Jack sets a "review" flag to ``+``, it appears
as :guilabel:`Jack: review [ + ]`.

A requested flag appears with the user who requested the flag prepended
to the flag name and the user who has been requested to set the flag
appended to the flag name within parentheses.  For example, if Jack
asks Jill for review, it appears as :guilabel:`Jack: review [ ? ] (Jill)`.

You can browse through open requests made of you and by you by selecting
:guilabel:`My Requests` from the footer. You can also look at open requests
limited by other requesters, requestees, products, components, and flag names.
Note that you can use '-' for requestee to specify flags with no requestee
set.

.. _flags-simpleexample:

A Simple Example
----------------

A developer might want to ask their manager,
"Should we fix this bug before we release version 2.0?"
They might want to do this for a *lot* of bugs,
so they decide to streamline the process. So:

#. The Bugzilla administrator creates a flag type called blocking2.0 for bugs
   in your product. It shows up on the :guilabel:`Show Bug` screen as the text
   :guilabel:`blocking2.0` with a drop-down box next to it. The drop-down box
   contains four values: an empty space, ``?``, ``-``, and ``+``.

#. The developer sets the flag to ``?``.

#. The manager sees the :guilabel:`blocking2.0`
   flag with a ``?`` value.

#. If the manager thinks the feature should go into the product
   before version 2.0 can be released, they set the flag to
   ``+``. Otherwise, they set it to ``-``.

#. Now, every Bugzilla user who looks at the bug knows whether or
   not the bug needs to be fixed before release of version 2.0.

.. _flags-about:

About Flags
-----------

Flags can have four values:

``?``
    A user is requesting that a status be set. (Think of it as 'A question is being asked'.)

``-``
    The status has been set negatively. (The question has been answered ``no``.)

``+``
    The status has been set positively.
    (The question has been answered ``yes``.)

``_``
    ``unset`` actually shows up as a blank space. This just means that nobody
    has expressed an opinion (or asked someone else to express an opinion)
    about the matter covered by this flag.

.. _flag-askto:

Flag Requests
-------------

If a flag has been defined as :guilabel:`requestable`, and a user has enough
privileges to request it (see below), the user can set the flag's status to
``?``. This status indicates that someone (a.k.a. "the requester") is asking
someone else to set the flag to either ``+`` or ``-``.

If a flag has been defined as :guilabel:`specifically requestable`,
a text box will appear next to the flag into which the requester may
enter a Bugzilla username. That named person (a.k.a. "the requestee")
will receive an email notifying them of the request, and pointing them
to the bug/attachment in question.

If a flag has *not* been defined as :guilabel:`specifically requestable`,
then no such text box will appear. A request to set this flag cannot be made
of any specific individual; these requests are open for anyone to answer. In
Bugzilla this is known as "asking the wind". A requester may ask the wind on
any flag simply by leaving the text box blank.

.. _flag-types:

.. _flag-type-attachment:

Attachment Flags
----------------

There are two types of flags: bug flags and attachment flags.

Attachment flags are used to ask a question about a specific
attachment on a bug.

Many Bugzilla installations use this to
request that one developer review another
developer's code before they check it in. They attach the code to
a bug report, and then set a flag on that attachment called
:guilabel:`review` to
:guilabel:`review? reviewer@example.com`.
reviewer\@example.com is then notified by email that
they have to check out that attachment and approve it or deny it.

For a Bugzilla user, attachment flags show up in three places:

#. On the list of attachments in the :guilabel:`Show Bug`
   screen, you can see the current state of any flags that
   have been set to ``?``, ``+``, or ``-``. You can see who asked about
   the flag (the requester), and who is being asked (the
   requestee).

#. When you edit an attachment, you can
   see any settable flag, along with any flags that have
   already been set. The :guilabel:`Edit Attachment`
   screen is where you set flags to ``?``, ``-``, ``+``, or unset them.

#. Requests are listed in the :guilabel:`Request Queue`, which
   is accessible from the :guilabel:`My Requests` link (if you are
   logged in) or :guilabel:`Requests` link (if you are logged out)
   visible on all pages.

.. _flag-type-bug:

Bug Flags
---------

Bug flags are used to set a status on the bug itself. You can
see Bug Flags in the :guilabel:`Show Bug` and :guilabel:`Requests`
screens, as described above.

Only users with enough privileges (see below) may set flags on bugs.
This doesn't necessarily include the assignee, reporter, or users with the
:group:`editbugs` permission.
