Components
==========

This part of the Bugzilla API allows you to deal with the available product
components. You will be able to get information about them as well as manipulate
them.

Create Component
----------------

This allows you to create a new component in Bugzilla. You must be authenticated
and be in the *editcomponents* group to perform this action.

**Request**

To create a new component:

.. code-block:: text

   POST /rest/component

.. code-block:: js

   {
     "product" : "TestProduct",
     "name" : "New Component",
     "description" : "This is a new component",
     "default_assignee" : "dkl@mozilla.com"
   }

Some params must be set, or an error will be thrown. These params are
shown in **bold**.

====================  =======  ==================================================
name                  type     description
====================  =======  ==================================================
**name**              string   The name of the new component.
**product**           string   The name of the product that the component must
                               be added to. This product must already exist, and
                               the user have the necessary permissions to edit
                               components for it.
**description**       string   The description of the new component.
**default_assignee**  string   The login name of the default assignee of the
                               component.
default_cc            array    Each string representing one login name of the
                               default CC list.
default_qa_contact    string   The login name of the default QA contact for the
                               component.
is_open               boolean  1 if you want to enable the component for bug
                               creations. 0 otherwise. Default is 1.
====================  =======  ==================================================

**Response**

.. code-block:: js

   {
     "id": 27
   }

====  ====  ========================================
name  type  description
====  ====  ========================================
id    int   The ID of the newly-added component.
====  ====  ========================================
