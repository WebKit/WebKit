.. _filing:

Filing a Bug
############

Reporting a New Bug
===================

Years of bug writing experience has been distilled for your
reading pleasure into the
`Bug Writing Guidelines <http://landfill.bugzilla.org/bugzilla-tip/page.cgi?id=bug-writing.html>`_.
While some of the advice is Mozilla-specific, the basic principles of
reporting Reproducible, Specific bugs and isolating the Product you are
using, the Version of the Product, the Component which failed, the Hardware
Platform, and Operating System you were using at the time of the failure go a
long way toward ensuring accurate, responsible fixes for the bug that bit you.

.. note:: If you want to file a test bug to see how Bugzilla works,
   you can do it on one of our test installations on
   `Landfill <http://landfill.bugzilla.org/>`_. Please don't do it on anyone's
   production Bugzilla installation.

The procedure for filing a bug is as follows:

#. Click the :guilabel:`New` link available in the header or footer
   of pages, or the :guilabel:`File a Bug` link on the home page.

#. First, you have to select the product in which you found a bug.

#. You now see a form where you can specify the component (part of
   the product which is affected by the bug you discovered; if you have
   no idea, just select :guilabel:`General` if such a component exists),
   the version of the program you were using, the operating system and
   platform your program is running on and the severity of the bug (if the
   bug you found crashes the program, it's probably a major or a critical
   bug; if it's a typo somewhere, that's something pretty minor; if it's
   something you would like to see implemented, then that's an enhancement).

#. You also need to provide a short but descriptive summary of the bug you found.
   "My program is crashing all the time" is a very poor summary
   and doesn't help developers at all. Try something more meaningful or
   your bug will probably be ignored due to a lack of precision.
   In the Description, give a detailed list of steps to reproduce
   the problem you encountered. Try to limit these steps to a minimum set
   required to reproduce the problem. This will make the life of
   developers easier, and the probability that they consider your bug in
   a reasonable timeframe will be much higher.

   .. note:: Try to make sure that everything in the Summary is also in the 
      Description. Summaries are often updated and this will ensure your original
      information is easily accessible.

#. As you file the bug, you can also attach a document (testcase, patch,
   or screenshot of the problem).

#. Depending on the Bugzilla installation you are using and the product in
   which you are filing the bug, you can also request developers to consider
   your bug in different ways (such as requesting review for the patch you
   just attached, requesting your bug to block the next release of the
   product, and many other product-specific requests).

#. Now is a good time to read your bug report again. Remove all misspellings;
   otherwise, your bug may not be found by developers running queries for some
   specific words, and so your bug would not get any attention.
   Also make sure you didn't forget any important information developers
   should know in order to reproduce the problem, and make sure your
   description of the problem is explicit and clear enough.
   When you think your bug report is ready to go, the last step is to
   click the :guilabel:`Submit Bug` button to add your report into the database.

.. _cloning-a-bug:

Clone an Existing Bug
=====================

Bugzilla allows you to "clone" an existing bug. The newly created bug will
inherit most settings from the old bug. This allows you to track similar
concerns that require different handling in a new bug. To use this, go to
the bug that you want to clone, then click the :guilabel:`Clone This Bug`
link on the bug page. This will take you to the :guilabel:`Enter Bug`
page that is filled with the values that the old bug has.
You can then change the values and/or text if needed.
