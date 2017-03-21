.. _quips:

Quips
#####

Quips are small user-defined messages (often quotes or witty sayings) that
can be configured to appear at the top of search results. Each Bugzilla
installation has its own specific quips. Whenever a quip needs to be
displayed, a random selection is made from the pool of already existing quips.

Quip submission is controlled by :param:`quip_list_entry_control`
parameter.  It has several possible values: open, moderated, or closed.
In order to enable quips approval you need to set this parameter to
"moderated". In this way, users are free to submit quips for addition,
but an administrator must explicitly approve them before they are
actually used.

In order to see the user interface for the quips, you can
click on a quip when it is displayed together with the search
results.  You can also go directly to the quips.cgi URL
(prefixed with the usual web location of the Bugzilla installation).
Once the quip interface is displayed, the "view and edit the whole
quip list" link takes you to the quips administration page, which
lists all quips available in the database.

Next to each quip there is a checkbox, under the
"Approved" column. Quips that have this checkbox checked are
already approved and will appear next to the search results.
The ones that have it unchecked are still preserved in the
database but will not appear on search results pages.
User submitted quips have initially the checkbox unchecked.

Also, there is a delete link next to each quip,
which can be used in order to permanently delete a quip.

Display of quips is controlled by the *display_quips*
user preference.  Possible values are "on" and "off".

