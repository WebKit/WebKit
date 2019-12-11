Comments
========

.. _rest_comments:

Get Comments
------------

This allows you to get data about comments, given a bug ID or comment ID.

**Request**

To get all comments for a particular bug using the bug ID or alias:

.. code-block:: text

   GET /rest/bug/(id_or_alias)/comment

To get a specific comment based on the comment ID:

.. code-block:: text

   GET /rest/bug/comment/(comment_id)

===============  ========  ======================================================
name             type      description
===============  ========  ======================================================
**id_or_alias**  mixed     A single integer bug ID or alias.
**comment_id**   int       A single integer comment ID.
new_since        datetime  If specified, the method will only return comments
                           *newer* than this time. This only affects comments
                           returned from the ``ids`` argument. You will always be
                           returned all comments you request in the
                           ``comment_ids`` argument, even if they are older than
                           this date.
===============  ========  ======================================================

**Response**

.. code-block:: js

   {
     "bugs": {
       "35": {
         "comments": [
           {
             "time": "2000-07-25T13:50:04Z",
             "text": "test bug to fix problem in removing from cc list.",
             "bug_id": 35,
             "count": 0,
             "attachment_id": null,
             "is_private": false,
             "tags": [],
             "creator": "user@bugzilla.org",
             "creation_time": "2000-07-25T13:50:04Z",
             "id": 75
           }
         ]
       }
     },
     "comments": {}
   }

Two items are returned:

``bugs`` This is used for bugs specified in ``ids``. This is an object,
where the keys are the numeric IDs of the bugs, and the value is
a object with a single key, ``comments``, which is an array of comments.
(The format of comments is described below.)

Any individual bug will only be returned once, so if you specify an ID
multiple times in ``ids``, it will still only be returned once.

``comments`` Each individual comment requested in ``comment_ids`` is
returned here, in a object where the numeric comment ID is the key,
and the value is the comment. (The format of comments is described below.)

A "comment" as described above is a object that contains the following items:

=============  ========  ========================================================
name           type      description
=============  ========  ========================================================
id             int       The globally unique ID for the comment.
bug_id         int       The ID of the bug that this comment is on.
attachment_id  int       If the comment was made on an attachment, this will be
                         the ID of that attachment. Otherwise it will be null.
count          int       The number of the comment local to the bug. The
                         Description is 0, comments start with 1.
text           string    The actual text of the comment.
creator        string    The login name of the comment's author.
time           datetime  The time (in Bugzilla's timezone) that the comment was
                         added.
creation_time  datetime  This is exactly same as the ``time`` key. Use this
                         field instead of ``time`` for consistency with other
                         methods including :ref:`rest_single_bug` and
                         :ref:`rest_attachments`.

                         For compatibility, ``time`` is still usable. However,
                         please note that ``time`` may be deprecated and removed
                         in a future release.

is_private     boolean   ``true`` if this comment is private (only visible to a
                         certain group called the "insidergroup"), ``false``
                         otherwise.
=============  ========  ========================================================

.. _rest_add_comment:

Create Comments
---------------

This allows you to add a comment to a bug in Bugzilla.

**Request**

To create a comment on a current bug.

.. code-block:: text

   POST /rest/bug/(id)/comment

.. code-block:: js

   {
     "comment" : "This is an additional comment",
     "is_private" : false
   }

===========  =======  ===========================================================
name         type     description
===========  =======  ===========================================================
**id**       int      The ID or alias of the bug to append a comment to.
**comment**  string   The comment to append to the bug. If this is empty
                      or all whitespace, an error will be thrown saying that you
                      did not set the ``comment`` parameter.
is_private   boolean  If set to true, the comment is private, otherwise it is
                      assumed to be public.
work_time    double   Adds this many hours to the "Hours Worked" on the bug.
                      If you are not in the time tracking group, this value will
                      be ignored.
===========  =======  ===========================================================

**Response**

.. code-block:: js

   {
     "id" : 789
   }

====  ====  =================================
name  type  description
====  ====  =================================
id    int   ID of the newly-created comment.
====  ====  =================================

.. _rest_search_comment_tags:

Search Comment Tags
-------------------

Searches for tags which contain the provided substring.

**Request**

To search for comment tags:

.. code-block:: text

   GET /rest/bug/comment/tags/(query)

Example:

.. code-block:: text

   GET /rest/bug/comment/tags/spa

=========  ======  ====================================================
name       type    description
=========  ======  ====================================================
**query**  string  Only tags containg this substring will be returned.
limit      int     If provided will return no more than ``limit`` tags.
                   Defaults to ``10``.
=========  ======  ====================================================

**Response**

.. code-block:: js

   [
     "spam"
   ]

An array of matching tags.

.. _rest_update_comment_tags:

Update Comment Tags
-------------------

Adds or removes tags from a comment.

**Request**

To update the tags comments attached to a comment:

.. code-block:: text

   PUT /rest/bug/comment/(comment_id)/tags

Example:

.. code-block:: js

   {
     "comment_id" : 75,
     "add" : ["spam", "bad"]
   }

==============  =====  ====================================
name            type   description
==============  =====  ====================================
**comment_id**  int    The ID of the comment to update.
add             array  The tags to attach to the comment.
remove          array  The tags to detach from the comment.
==============  =====  ====================================

**Response**

.. code-block:: js

   [
     "bad",
     "spam"
   ]

An array of strings containing the comment's updated tags.
