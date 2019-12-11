.. _workflow:

Workflow
########

The bug status workflow—which statuses are valid transitions from which
other statuses—can be customized.

You need to begin by defining the statuses and resolutions you want to use
(see :ref:`field-values`). By convention, these are in all capital letters.

Only one bug status, UNCONFIRMED, can never be renamed nor deleted. However,
it can be disabled entirely on a per-product basis (see :ref:`categorization`).
The status referred to by the :param:`duplicate_or_move_bug_status` parameter, if
set, is also undeletable. To make it deletable,
simply set the value of that parameter to a different status.

Aside from the empty value, two resolutions, DUPLICATE and FIXED, cannot be
renamed or deleted. (FIXED could be if we fixed
`bug 1007605 <https://bugzilla.mozilla.org/show_bug.cgi?id=1007605>`_.)

Once you have defined your statuses, you can configure the workflow of
how a bug moves between them. The workflow configuration
page displays all existing bug statuses twice: first on the left for the
starting status, and on the top for the target status in the transition.
If the checkbox is checked, then the transition from the left to the top
status is legal; if it's unchecked, that transition is forbidden.

The status used as the :param:`duplicate_or_move_bug_status` parameter
(normally RESOLVED or its equivalent) is required to be a legal transition
from every other bug status, and so this is enforced on the page.

The "View Comments Required on Status Transitions" link below the table
lets you set which transitions require a comment from the user.
