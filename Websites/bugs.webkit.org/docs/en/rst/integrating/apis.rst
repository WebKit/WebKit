.. _api-list:

APIs
####

Bugzilla has a number of APIs that you can call in your code to extract
information from and put information into Bugzilla. Some are deprecated and
will soon be removed. Which one to use? Short answer: the
:ref:`REST WebService API v1 <apis>`
should be used for all new integrations, but keep an eye out for version 2,
coming soon.

The APIs currently available are as follows:

Core Module API
===============

Most of the core Bugzilla modules have extensive documentation inside the modules
themselves. You can view the :api:`POD documentation <index.html>` to help with
using the core modules in your extensions.

Ad-Hoc APIs
===========

Various pages on Bugzilla are available in machine-parseable formats as well
as HTML. For example, bugs can be downloaded as XML, and buglists as CSV.
CSV is useful for spreadsheet import. There should be links on the HTML page
to alternate data formats where they are available.

XML-RPC
=======

Bugzilla has an :api:`XML-RPC API <Bugzilla/WebService/Server/XMLRPC.html>`.
This will receive no further updates and will be removed in a future version
of Bugzilla.

Endpoint: :file:`/xmlrpc.cgi`

JSON-RPC
========

Bugzilla has a :api:`JSON-RPC API <Bugzilla/WebService/Server/JSONRPC.html>`.
This will receive no further updates and will be removed in a future version
of Bugzilla.

Endpoint: :file:`/jsonrpc.cgi`

REST
====

Bugzilla has a :ref:`REST API <apis>` which is the currently-recommended API
for integrating with Bugzilla. The current REST API is version 1. It is stable,
and so will not be changed in a backwardly-incompatible way.

**This is the currently-recommended API for new development.**

Endpoint: :file:`/rest`

BzAPI/BzAPI-Compatible REST
===========================

The first ever REST API for Bugzilla was implemented using an external proxy
called `BzAPI <https://wiki.mozilla.org/Bugzilla:BzAPI>`_. This became popular
enough that a BzAPI-compatible shim on top of the (native) REST API has been
written, to allow code which used the BzAPI API to take advantage of the
speed improvements of direct integration without needing to be rewritten.
The shim is an extension which you would need to install in your Bugzilla.

Neither BzAPI nor this BzAPI-compatible API shim will receive any further
updates, and they should not be used for new code.

REST v2
=======

The future of Bugzilla's APIs is version 2 of the REST API, which will take
the best of the current REST API and the BzAPI API. It is still under
development.
