.. _flags-admin:

Flags
#####

If you have the :group:`editcomponents` permission, you can
edit Flag Types from the main administration page. Clicking the
:guilabel:`Flags` link will bring you to the :guilabel:`Administer
Flag Types` page. Here, you can select whether you want
to create (or edit) a Bug flag or an Attachment flag.

The two flag types have the same administration interface, and the interface
for creating a flag and editing a flag have the same set of fields.

.. _flags-edit:

Flag Properties
===============

Name
    This is the name of the flag. This will be displayed
    to Bugzilla users who are looking at or setting the flag.
    The name may contain any valid Unicode characters except commas
    and spaces.

Description
    The description describes the flag in more detail. It is visible
    in a tooltip when hovering over a flag either in the :guilabel:`Show Bug`
    or :guilabel:`Edit Attachment` pages. This field can be as
    long as you like and can contain any character you want.

Category
    You can set a flag to be visible or not visible on any combination of
    products and components.

    Default behaviour for a newly created flag is to appear on all
    products and all components, which is why ``__Any__:__Any__``
    is already entered in the :guilabel:`Inclusions` box.
    If this is not your desired behaviour, you must either set some
    exclusions (for products on which you don't want the flag to appear),
    or you must remove ``__Any__:__Any__`` from the :guilabel:`Inclusions` box
    and define products/components specifically for this flag.

    To create an Inclusion, select a Product from the top drop-down box.
    You may also select a specific component from the bottom drop-down box.
    (Setting ``__Any__`` for Product translates to
    "all the products in this Bugzilla".
    Selecting  ``__Any__`` in the Component field means
    "all components in the selected product.")
    Selections made, press :guilabel:`Include`, and your
    Product/Component pairing will show up in the :guilabel:`Inclusions` box on the right.

    To create an Exclusion, the process is the same: select a Product from the
    top drop-down box, select a specific component if you want one, and press
    :guilabel:`Exclude`. The Product/Component pairing will show up in the
    :guilabel:`Exclusions` box on the right.

    This flag *will* appear and *can* be set for any
    products/components appearing in the :guilabel:`Inclusions` box
    (or which fall under the appropriate ``__Any__``).
    This flag *will not* appear (and therefore *cannot* be set) on
    any products appearing in the :guilabel:`Exclusions` box.
    *IMPORTANT: Exclusions override inclusions.*

    You may select a Product without selecting a specific Component,
    but you cannot select a Component without a Product. If you do so,
    Bugzilla will display an error message, even if all your products
    have a component by that name. You will also see an error if you
    select a Component that does not belong to the selected Product.

    *Example:* Let's say you have a product called
    ``Jet Plane`` that has thousands of components. You want
    to be able to ask if a problem should be fixed in the next model of
    plane you release. We'll call the flag ``fixInNext``.
    However, one component in ``Jet Plane`` is
    called ``Pilot``, and it doesn't make sense to release a
    new pilot, so you don't want to have the flag show up in that component.
    So, you include ``Jet Plane:__Any__`` and you exclude
    ``Jet Plane:Pilot``.

Sort Key
    Flags normally show up in alphabetical order. If you want them to
    show up in a different order, you can use this key set the order on each flag.
    Flags with a lower sort key will appear before flags with a higher
    sort key. Flags that have the same sort key will be sorted alphabetically.

Active
    Sometimes you might want to keep old flag information in the
    Bugzilla database but stop users from setting any new flags of this type.
    To do this, uncheck :guilabel:`active`. Deactivated
    flags will still show up in the UI if they are ``?``, ``+``, or ``-``, but
    they may only be cleared (unset) and cannot be changed to a new value.
    Once a deactivated flag is cleared, it will completely disappear from a
    bug/attachment and cannot be set again.

Requestable
    New flags are, by default, "requestable", meaning that they
    offer users the ``?`` option, as well as ``+``
    and ``-``.
    To remove the ``?`` option, uncheck "requestable".

Specifically Requestable
    By default this box is checked for new flags, meaning that users may make
    flag requests of specific individuals. Unchecking this box will remove the
    text box next to a flag; if it is still requestable, then requests
    cannot target specific users and are open to anyone (called a
    request "to the wind" in Bugzilla). Removing this after specific
    requests have been made will not remove those requests; that data will
    stay in the database (though it will no longer appear to the user).

Multiplicable
    Any flag with :guilabel:`Multiplicable:guilabel:` set (default for new flags
    is 'on') may be set more than once. After being set once, an unset flag
    of the same type will appear below it with "addl." (short for
    "additional") before the name. There is no limit to the number of
    times a Multiplicable flags may be set on the same bug/attachment.

CC List
    If you want certain users to be notified every time this flag is
    set to ``?``, ``-``, or ``+``, or is unset, add them here. This is a comma-separated
    list of email addresses that need not be restricted to Bugzilla usernames.

Grant Group
    When this field is set to some given group, only users in the group
    can set the flag to ``+`` and ``-``. This
    field does not affect who can request or cancel the flag. For that,
    see the :guilabel:`Request Group` field below. If this field
    is left blank, all users can set or delete this flag. This field is
    useful for restricting which users can approve or reject requests.

Request Group
    When this field is set to some given group, only users in the group
    can request or cancel this flag. Note that this field has no effect
    if the :guilabel:`Grant Group` field is empty. You can set the
    value of this field to a different group, but both fields have to be
    set to a group for this field to have an effect.

.. _flags-delete:

Deleting a Flag
===============

When you are at the :guilabel:`Administer Flag Types` screen,
you will be presented with a list of Bug flags and a list of Attachment
Flags.

To delete a flag, click on the :guilabel:`Delete` link next to
the flag description.

.. warning:: Once you delete a flag, it is *gone* from
   your Bugzilla. All the data for that flag will be deleted.
   Everywhere that flag was set, it will disappear,
   and you cannot get that data back. If you want to keep flag data,
   but don't want anybody to set any new flags or change current flags,
   unset :guilabel:`active` in the flag Edit form.
