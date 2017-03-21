Classifications
===============

This part of the Bugzilla API allows you to deal with the available
classifications. You will be able to get information about them as well as
manipulate them.

.. _rest_get_classification:

Get Classification
------------------

Returns an object containing information about a set of classifications.

**Request**

To return information on a single classification using the ID or name:

.. code-block:: text

   GET /rest/classification/(id_or_name)

==============  =====  =====================================
name            type   description
==============  =====  =====================================
**id_or_name**  mixed  An Integer classification ID or name.
==============  =====  =====================================

**Response**

.. code-block:: js

   {
     "classifications": [
       {
         "sort_key": 0,
         "description": "Unassigned to any classifications",
         "products": [
           {
             "id": 2,
             "name": "FoodReplicator",
             "description": "Software that controls a piece of hardware that will create any food item through a voice interface."
           },
           {
             "description": "Silk, etc.",
             "name": "Spider Secretions",
             "id": 4
           }
         ],
         "id": 1,
         "name": "Unclassified"
       }
     ]
   }

``classifications`` (array) Each object is a classification that the user is
authorized to see and has the following items:

===========  ======  ============================================================
name         type    description
===========  ======  ============================================================
id           int     The ID of the classification.
name         string  The name of the classification.
description  string  The description of the classificaion.
sort_key     int     The value which determines the order the classification is
                     sorted.
products     array   Products the user is authorized to
                     access within the classification. Each hash has the
                     following keys:
===========  ======  ============================================================

Product object:

===========  ======  ================================
name         type    description
===========  ======  ================================
name         string  The name of the product.
id           int     The ID of the product.
description  string  The description of the product.
===========  ======  ================================
