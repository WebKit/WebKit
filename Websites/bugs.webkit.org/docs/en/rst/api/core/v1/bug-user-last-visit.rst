Bug User Last Visited
=====================

.. _rest-bug-user-last-visit-update:

Update Last Visited
-------------------

Update the last-visited time for the specified bug and current user.

**Request**

To update the time for a single bug id:

.. code-block:: text

   POST /rest/bug_user_last_visit/(id)

To update one or more bug ids at once:

.. code-block:: text

   POST /rest/bug_user_last_visit

.. code-block:: js

   {
     "ids" : [35,36,37]
   }

=======  =====  ==============================
name     type   description
=======  =====  ==============================
**id**   int    An integer bug id.
**ids**  array  One or more bug ids to update.
=======  =====  ==============================

**Response**

.. code-block:: js

   [
     {
       "id" : 100,
       "last_visit_ts" : "2014-10-16T17:38:24Z"
     }
   ]

An array of objects containing the items:

=============  ========  ============================================
name           type      description
=============  ========  ============================================
id             int       The bug id.
last_visit_ts  datetime  The timestamp the user last visited the bug.
=============  ========  ============================================

.. _rest-bug-user-last-visit-get:

Get Last Visited
----------------

**Request**

Get the last-visited timestamp for one or more specified bug ids or get a
list of the last 20 visited bugs and their timestamps.

To return the last-visited timestamp for a single bug id:

.. code-block:: text

   GET /rest/bug_user_last_visit/(id)

To return more than one specific bug timestamps:

.. code-block:: text

   GET /rest/bug_user_last_visit/123?ids=234&ids=456

To return just the most recent 20 timestamps:

.. code-block:: text

   GET /rest/bug_user_last_visit

=======  =====  ============================================
name     type   description
=======  =====  ============================================
**id**   int    An integer bug id.
**ids**  array  One or more optional bug ids to get.
=======  =====  ============================================

**Response**

.. code-block:: js

   [
     {
       "id" : 100,
       "last_visit_ts" : "2014-10-16T17:38:24Z"
     }
   ]

An array of objects containing the following items:

=============  ========  ============================================
name           type      description
=============  ========  ============================================
id             int       The bug id.
last_visit_ts  datetime  The timestamp the user last visited the bug.
=============  ========  ============================================
