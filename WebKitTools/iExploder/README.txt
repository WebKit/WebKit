iExploder 1.2.0
===============

Welcome to iExploder. a highly inefficient, but fairly effective web
browser tester. The code still has a lot of work to be done, but it's
definitely usable. Here are some notable features:

* Tests all common HTML and CSS tags and attributes, as parsed from 
the KHTML and Mozilla source trees, as well as tags for Internet Explorer
from MSDN. This also includes a few Javascript hooks.
* Numeric, and String overflow and formatting tests
* Sequential and Randomized Test Case Generation
* Test Case Lookups
* Subtest generation (very weak right now)



Installation instructions:
--------------------------
Make sure you have Ruby installed (comes with Mac OS X, most Linux
distributions). See http://www.ruby-lang.org/ if you do not.

Copy the contents of the htdocs/ folder to any directory served
by your webserver. Make sure that directory can execute CGI scripts.



FAQ:
----
1) Does it work with mod_ruby?

  Yes, it's actually recommended for boosting performance. 

2) Are the tests always the same? 

  The test cases should always be the same on a single installation, but not
necessarily on different installations of iExploder. Random generator seeds
may differ between operating systems and platforms.


3) How do I look up the last successful test for a client?

  Use tools/lasthit.rb. When I get a crash, I usually do something like:

tail -15000 /var/log/apache2/access_log | ./lasthit.rb

Letting you know how many tests and what the last test id was for each
client tested. You can then try to repeat the test, or go through the
subtests to see if you can repeat the crash.

4) How do subtests work?

  Pretty badly right now. If you have a page that crashes, you can resubmit
it with subtest=1, iexploder will try to pick out the particular tag that
crashes your browser (up to $maxTags). Often times, none of the subtests
will crash the browser, but the entire test will. This has a lot of room for
improvement.

5) How come I can't seem to repeat the crash?

  Many browser crashes are race conditions that are not easy to repeat. Some
crashes only happen when going from test 4 -> test 5 -> test 6. If you can't
repeat the crash through subtests or a lookup of the failing test, try going
back a few tests.

It is also possible (but unliely) that your browser crashed while trying to load
the next test in the list, and your webserver did not log that hit yet.

6) Why did you write this?

  I wanted to make sure that FireFox had as many bugs fixed in it as possible
before the 1.0 release. After 1.0 came out, I kept improving it.

7) Why is the code so awful?

  I wrote this as a quick hack sitting in a hotel room one day while I was
waiting for some people to show up. It now represents the worst Ruby code
I have ever written. Heck, it looks as bad as Perl!

7) Why does Internet Explorer run the tests so slowly?

  <META> refresh tags are very fragile in Internet Explorer, and can be easily
be rendered useless by other tags on the page. If this happens, a javascript
refresh will execute after a 1 second delay. 
