General
=======

This is the standard REST API for external programs that want to interact
with Bugzilla. It provides a REST interface to various Bugzilla functions.

Basic Information
-----------------

**Browsing**

If the ``Accept`` header of a request is set to ``text/html`` (as it is by an
ordinary web browser) then the API will return the JSON data as a HTML
page which the browser can display. In other words, you can play with the
API using just your browser to see results in a human-readable form.
This is a good way to try out the various GET calls, even if you can't use
it for POST or PUT.

**Data Format**

The REST API only supports JSON input, and either JSON or JSONP output.
So objects sent and received must be in JSON format.

On every request, you must set both the ``Accept`` and ``Content-Type`` HTTP
headers to the MIME type of the data format you are using to communicate with
the API. ``Content-Type`` tells the API how to interpret your request, and
``Accept`` tells it how you want your data back. ``Content-Type`` must be
``application/json``. ``Accept`` can be either that, or
``application/javascript`` for JSONP. In the latter`case, add a ``callback``
parameter to name your callback.

Parameters may also be passed in as part of the query string for non-GET
requests and will override any matching parameters in the request body.

Example request which returns the current version of Bugzilla:

.. code-block:: http

   GET /rest/version HTTP/1.1
   Host: bugzilla.example.com
   Accept: application/json

Example response:

.. code-block:: http

   HTTP/1.1 200 OK
   Vary: Accept
   Content-Type: application/json

   {
     "version" : "4.2.9+"
   }

**Errors**

When an error occurs over REST, an object is returned with the key ``error``
set to ``true``.

The error contents look similar to:

.. code-block:: js

   {
     "error": true,
     "message": "Some message here",
     "code": 123
   }

Common Data Types
-----------------

The Bugzilla API uses the following various types of parameters:

========  ======================================================================
 type     description
========  ======================================================================
int       Integer.
double    A floating-point number.
string    A string.
email     A string representing an email address. This value, when returned,
          may be filtered based on if the user is logged in or not.
date      A specific date. Example format: ``YYYY-MM-DD``.
datetime  A date/time. Timezone should be in UTC unless otherwise noted.
          Example format: ``YYYY-MM-DDTHH24:MI:SSZ``.
boolean   ``true`` or ``false``.
base64    A base64-encoded string. This is the only way to transfer
          binary data via the API.
array     An array. There may be mixed types in an array. ``[`` and ``]`` are
          used to represent the beginning and end of arrays.
object    A mapping of keys to values. Called a "hash", "dict", or "map" in
          some other programming languages. The keys are strings, and the
          values can be any type. ``{`` and ``}`` are used to represent the
          beginning and end of objects.
========  ======================================================================

Parameters that are required will be displayed in **bold** in the parameters
table for each API method.

Authentication
--------------

Some methods do not require you to log in. An example of this is
:ref:`rest_single_bug`. However, authenticating yourself allows you to see
non-public information, for example, a bug that is not publicly visible.

There are two ways to authenticate yourself:

**API Keys**

You can specify ``Bugzilla_api_key`` or simply ``api_key`` as an argument to
any call, and you will be logged in as that user if the key is correct and has
not been revoked. You can set up an API key by using the 'API Key' tab in the
Preferences pages.

**Login and Password**

You can specify ``Bugzilla_login`` and ``Bugzilla_password`` or simply
``login`` and ``password`` respectively, as arguments to any call, and you will
be logged in as that user if your credentials are correct.

======================  =======  ==============================================
name                    type     description
======================  =======  ==============================================
**Bugzilla_login**      string   A user's login name.
**Bugzilla_password**   string   That user's password.
Bugzilla_restrictlogin  boolean  If true, then your login will only be
                                 valid for your IP address.
======================  =======  ==============================================

The ``Bugzilla_restrictlogin`` option is only used when you have also
specified ``Bugzilla_login`` and ``Bugzilla_password``.

There is also a deprecated method of authentication described below that will be
removed in the version after Bugzilla 5.0.

**Bugzilla Tokens**

You can use :ref:`rest_user_login` to log in as a Bugzilla user. This issues a
token that you must then use in future calls. Just use the value for ``token``
and pass as either ``Bugzilla_token`` or simply ``token`` as arguments to an
API call.

==================  ======  ===================================================
name                type    description
==================  ======  ===================================================
**Bugzilla_token**  string  You can specify this as argument to any call,
                            and you will be logged in as that user if the
                            token is correct. This is the token returned
                            when calling :ref:`rest_user_login` mentioned
                            above.
==================  ======  ===================================================

An error is thrown if you pass an invalid token; you will need to log in again
to get a new token.

Also starting with Bugzilla 5.0, login cookies are no longer returned by
:ref:`rest_user_login` due to security concerns.

Useful Parameters
-----------------

Many calls take common arguments. These are documented below and linked from
the individual calls where these parameters are used.

**Including Fields**

Many calls return an array of objects with various fields in the objects. (For
example, :ref:`rest_single_bug` returns a list of ``bugs`` that have fields like
``id``, ``summary``,  ``creation_time``, etc.)

These parameters allow you to limit what fields are present in the objects, to
improve performance or save some bandwidth.

``include_fields``: The (case-sensitive) names of fields in the response data.
Only the fields specified in the object will be returned, the rest will not be
included. Fields should be comma delimited.

Invalid field names are ignored.

Example request to :ref:`rest_user_get`:

.. code-block:: text

   GET /rest/user/1?include_fields=id,name

would return something like:

.. code-block:: js

   {
     "users" : [
       {
         "id" : 1,
         "name" : "user@domain.com"
       }
     ]
   }

**Excluding Fields**

``exclude_fields``: The (case-sensitive) names of fields in the return value. The\
fields specified will not be included in the returned hashes. Fields should
be comma delimited.

Invalid field names are ignored.

Specifying fields here overrides ``include_fields``, so if you specify a
field in both, it will be excluded, not included.

Example request to :ref:`rest_user_get`:

.. code-block:: js

   GET /rest/user/1?exclude_fields=name

would return something like:

.. code-block:: js

   {
     "users" : [
       {
         "id" : 1,
         "real_name" : "John Smith"
       }
     ]
   }

Some calls support specifying "subfields". If a call states that it supports
"subfield" restrictions, you can restrict what information is returned within
the first field. For example, if you call :ref:`rest_product_get` with an
``include_fields`` of ``components.name``, then only the component name would be
returned (and nothing else). You can include the main field, and exclude a
subfield.

There are several shortcut identifiers to ask for only certain groups of
fields to be returned or excluded:

=========  =====================================================================
value      description
=========  =====================================================================
_all       All possible fields are returned if this is specified in
           ``include_fields``.
_default   Default fields are returned if ``include_fields`` is empty or
           this is specified. This is useful if you want the default
           fields in addition to a field that is not normally returned.
_extra     Extra fields are not returned by default and need to be manually
           specified in ``include_fields`` either by exact field name, or adding
           ``_extra``.
 _custom   Custom fields are normally returned by default unless this is added
           to ``exclude_fields``. Also you can use it in ``include_fields`` if
           for example you want specific field names plus all custom fields.
           Custom fields are normally only relevant to bug objects.
=========  =====================================================================
