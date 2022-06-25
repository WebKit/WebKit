Bugzilla Information
====================

These methods are used to get general configuration information about this
Bugzilla instance.

Version
-------

Returns the current version of Bugzilla. Normally in the format of ``X.X`` or
``X.X.X``. For example, ``4.4`` for the initial release of a new branch. Or
``4.4.6`` for a minor release on the same branch.

**Request**

.. code-block:: text

   GET /rest/version

**Response**

.. code-block:: js

   {
     "version": "4.5.5+"
   }

=======  ======  =========================================
name     type    description
=======  ======  =========================================
version  string  The current version of this Bugzilla
=======  ======  =========================================

Extensions
----------

Gets information about the extensions that are currently installed and enabled
in this Bugzilla.

**Request**

.. code-block:: text

   GET /rest/extensions

**Response**

.. code-block:: js

   {
     "extensions": {
       "Voting": {
         "version": "4.5.5+"
       },
       "BmpConvert": {
         "version": "1.0"
       }
     }
   }

==========  ======  ====================================================
name        type    description
==========  ======  ====================================================
extensions  object  An object containing the extensions enabled as keys.
                    Each extension object contains the following keys:

                    * ``version`` (string) The version of the extension.
==========  ======  ====================================================

Timezone
--------

Returns the timezone in which Bugzilla expects to receive dates and times on the API.
Currently hard-coded to UTC ("+0000"). This is unlikely to change.

**Request**

.. code-block:: text

   GET /rest/timezone

.. code-block:: js

   {
     "timezone": "+0000"
   }

**Response**

========  ======  ===============================================================
name      type    description
========  ======  ===============================================================
timezone  string  The timezone offset as a string in (+/-)XXXX (RFC 2822) format.
========  ======  ===============================================================

Time
----

Gets information about what time the Bugzilla server thinks it is, and
what timezone it's running in.

**Request**

.. code-block:: text

   GET /rest/time

**Response**

.. code-block:: js

   {
     "web_time_utc": "2014-09-26T18:01:30Z",
     "db_time": "2014-09-26T18:01:30Z",
     "web_time": "2014-09-26T18:01:30Z",
     "tz_offset": "+0000",
     "tz_short_name": "UTC",
     "tz_name": "UTC"
   }

=============  ======  ==========================================================
name           type    description
=============  ======  ==========================================================
db_time        string  The current time in UTC, according to the Bugzilla
                       database server.

                       Note that Bugzilla assumes that the database and the
                       webserver are running in the same time zone. However,
                       if the web server and the database server aren't
                       synchronized or some reason, *this* is the time that
                       you should rely on or doing searches and other input
                       to the WebService.
web_time       string  This is the current time in UTC, according to
                       Bugzilla's web server.

                       This might be different by a second from ``db_time``
                       since this comes from a different source. If it's any
                       more different than a second, then there is likely
                       some problem with this Bugzilla instance. In this
                       case you should rely  on the ``db_time``, not the
                       ``web_time``.
web_time_utc   string  Identical to ``web_time``. (Exists only for
                       backwards-compatibility with versions of Bugzilla
                       before 3.6.)
tz_name        string  The literal string ``UTC``. (Exists only for
                       backwards-compatibility with versions of Bugzilla
                       before 3.6.)
tz_short_name  string  The literal string ``UTC``. (Exists only for
                       backwards-compatibility with versions of Bugzilla
                       before 3.6.)
tz_offset      string  The literal string ``+0000``. (Exists only for
                       backwards-compatibility with versions of Bugzilla
                       before 3.6.)
=============  ======  ==========================================================

Parameters
----------

Returns parameter values currently used in this Bugzilla.

**Request**

.. code-block:: text

   GET /rest/parameters

**Response**

Example response for anonymous user:

.. code-block:: js

   {
      "parameters" : {
         "maintainer" : "admin@example.com",
         "requirelogin" : "0"
      }
   }

Example response for authenticated user:

.. code-block:: js

   {
      "parameters" : {
          "allowemailchange" : "1",
          "attachment_base" : "http://bugzilla.example.com/",
          "commentonchange_resolution" : "0",
          "commentonduplicate" : "0",
          "cookiepath" : "/",
          "createemailregexp" : ".*",
          "defaultopsys" : "",
          "defaultplatform" : "",
          "defaultpriority" : "--",
          "defaultseverity" : "normal",
          "duplicate_or_move_bug_status" : "RESOLVED",
          "emailregexp" : "^[\\w\\.\\+\\-=']+@[\\w\\.\\-]+\\.[\\w\\-]+$",
          "emailsuffix" : "",
          "letsubmitterchoosemilestone" : "1",
          "letsubmitterchoosepriority" : "1",
          "mailfrom" : "bugzilla-daemon@example.com",
          "maintainer" : "admin@example.com",
          "maxattachmentsize" : "1000",
          "maxlocalattachment" : "0",
          "musthavemilestoneonaccept" : "0",
          "noresolveonopenblockers" : "0",
          "password_complexity" : "no_constraints",
          "rememberlogin" : "on",
          "requirelogin" : "0",
          "urlbase" : "http://bugzilla.example.com/",
          "use_see_also" : "1",
          "useclassification" : "1",
          "usemenuforusers" : "0",
          "useqacontact" : "1",
          "usestatuswhiteboard" : "1",
          "usetargetmilestone" : "1",
      }
   }

A logged-out user can only access the ``maintainer`` and ``requirelogin``
parameters.

A logged-in user can access the following parameters (listed alphabetically):

* allowemailchange
* attachment_base
* commentonchange_resolution
* commentonduplicate
* cookiepath
* defaultopsys
* defaultplatform
* defaultpriority
* defaultseverity
* duplicate_or_move_bug_status
* emailregexpdesc
* emailsuffix
* letsubmitterchoosemilestone
* letsubmitterchoosepriority
* mailfrom
* maintainer
* maxattachmentsize
* maxlocalattachment
* musthavemilestoneonaccept
* noresolveonopenblockers
* password_complexity
* rememberlogin
* requirelogin
* search_allow_no_criteria
* urlbase
* use_see_also
* useclassification
* usemenuforusers
* useqacontact
* usestatuswhiteboard
* usetargetmilestone

A user in the tweakparams group can access all existing parameters.
New parameters can appear or obsolete parameters can disappear depending
on the version of Bugzilla and on extensions being installed.
The list of parameters returned by this method is not stable and will
never be stable.

Last Audit Time
---------------

Gets the most recent timestamp among all of the events recorded in the audit_log
table.

**Request**

To get most recent audit timestamp for all classes:

.. code-block:: text

   GET /rest/last_audit_time

To get the the most recent audit timestamp for the ``Bugzilla::Product`` class:

.. code-block:: text

   GET /rest/last_audit_time?class=Bugzilla::Product

=====  =====  ===================================================================
name   type   description
=====  =====  ===================================================================
class  array  The class names are defined as ``Bugzilla::<class_name>"`` such as
              Bugzilla:Product`` for products.
=====  =====  ===================================================================

**Response**

.. code-block:: js

   {
     "last_audit_time": "2014-09-23T18:03:38Z"
   }

===============  ======  ====================================================
name             type    description
===============  ======  ====================================================
last_audit_time  string  The maximum of the at_time from the audit_log.
===============  ======  ====================================================
