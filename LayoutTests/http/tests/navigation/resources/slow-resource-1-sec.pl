#!/usr/bin/perl -w

# flush the buffers after each print
select (STDOUT);
$| = 1;

sleep 1;

print "Content-Type: text/plain\n";
print "Expires: Thu, 01 Dec 2003 16:00:00 GMT\n";
print "Cache-Control: no-store, no-cache, must-revalidate\n";
print "Pragma: no-cache\n";
print "\n";

print "This resource<br>\n";
print "Only loads<br>\n";
print "After a second<br>\n";
print "Of delay<br>\n";
print "<br>\n";
print "<br>\n";
print "<br>\n";
print "<br>\n";
print "<br>\n";
print "<br>\n";
print "<br>\n";
print "<br>\n";
print "<br>\n";
print "<br>\n";

