.. _reports-and-charts:

Reports and Charts
##################

As well as the standard buglist, Bugzilla has two more ways of
viewing sets of bugs. These are the reports (which give different
views of the current state of the database) and charts (which plot
the changes in particular sets of bugs over time).

.. _reports:

Reports
=======

A report is a view of the current state of the bug database.

You can run either an HTML-table-based report, or a graphical
line/pie/bar-chart-based one. The two have different pages to
define them but are close cousins - once you've defined and
viewed a report, you can switch between any of the different
views of the data at will.

Both report types are based on the idea of defining a set of bugs
using the standard search interface and then choosing some
aspect of that set to plot on the horizontal and/or vertical axes.
You can also get a form of 3-dimensional report by choosing to have
multiple images or tables.

So, for example, you could use the search form to choose "all
bugs in the WorldControl product" and then plot their severity
against their component to see which component had had the largest
number of bad bugs reported against it.

Once you've defined your parameters and hit :guilabel:`Generate Report`,
you can switch between HTML, CSV, Bar, Line and Pie. (Note: Pie
is only available if you didn't define a vertical axis, as pie
charts don't have one.) The other controls are fairly self-explanatory;
you can change the size of the image if you find text is overwriting
other text, or the bars are too thin to see.

.. _charts:

Charts
======

A chart is a view of the state of the bug database over time.

Bugzilla currently has two charting systems - Old Charts and New
Charts. Old Charts have been part of Bugzilla for a long time; they
chart each status and resolution for each product, and that's all.
They are deprecated, and going away soon - we won't say any more
about them.
New Charts are the future - they allow you to chart anything you
can define as a search.

.. note:: Both charting forms require the administrator to set up the
   data-gathering script. If you can't see any charts, ask them whether
   they have done so.

An individual line on a chart is called a data set.
All data sets are organised into categories and subcategories. The
data sets that Bugzilla defines automatically use the Product name
as a :guilabel:`Category` and Component names as :guilabel:`Subcategories`,
but there is no need for you to follow that naming scheme with your own
charts if you don't want to.

Data sets may be public or private. Everyone sees public data sets in
the list, but only their creator sees private data sets. Only
administrators can make data sets public.
No two data sets, even two private ones, can have the same set of
category, subcategory and name. So if you are creating private data
sets, one idea is to have the :guilabel:`Category` be your username.

Creating Charts
---------------

You create a chart by selecting a number of data sets from the
list and pressing :guilabel:`Add To List` for each. In the
:guilabel:`List Of Data Sets To Plot`, you can define the label that data
set will have in the chart's legend and also ask Bugzilla to :guilabel:`Sum`
a number of data sets (e.g. you could :guilabel:`Sum` data sets representing
:guilabel:`RESOLVED`, :guilabel:`VERIFIED` and :guilabel:`CLOSED` in a
particular product to get a data set representing all the resolved bugs in
that product.)

If you've erroneously added a data set to the list, select it
using the checkbox and click :guilabel:`Remove`. Once you add more than one
data set, a :guilabel:`Grand Total` line
automatically appears at the bottom of the list. If you don't want
this, simply remove it as you would remove any other line.

You may also choose to plot only over a certain date range, and
to cumulate the results, that is, to plot each one using the
previous one as a baseline so the top line gives a sum of all
the data sets. It's easier to try than to explain :-)

Once a data set is in the list, you can also perform certain
actions on it. For example, you can edit the
data set's parameters (name, frequency etc.) if it's one you
created or if you are an administrator.

Once you are happy, click :guilabel:`Chart This List` to see the chart.

.. _charts-new-series:

Creating New Data Sets
----------------------

You may also create new data sets of your own. To do this,
click the :guilabel:`create a new data set` link on the
:guilabel:`Create Chart` page. This takes you to a search-like interface
where you can define the search that Bugzilla will plot. At the bottom of the
page, you choose the category, sub-category and name of your new
data set.

If you have sufficient permissions, you can make the data set public,
and reduce the frequency of data collection to less than the default
of seven days.

