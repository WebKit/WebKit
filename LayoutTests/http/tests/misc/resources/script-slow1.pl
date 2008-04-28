#!/usr/bin/perl -w

# flush the buffers after each print
select (STDOUT);
$| = 1;

print "Content-Type: application/javascript\r\n";
print "Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n";
print "Cache-Control: no-store, no-cache, must-revalidate\r\n";
print "Pragma: no-cache\r\n";
print "\r\n";

sleep 2;
print "var i = 3;\n";
