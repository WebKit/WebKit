#!/usr/bin/perl -wT

# flush the buffers after each print
select (STDOUT);
$| = 1;

print "Content-Type: image/jpeg\r\n";
print "Expires: Thu, 01 Dec 2003 16:00:00 GMT\r\n";
print "Cache-Control: no-store, no-cache, must-revalidate\r\n";
print "Pragma: no-cache\r\n";
print "\r\n";

sleep 2;

open(FILE, "compass.jpg");
while (<FILE>) {
  print $_;
}
close(FILE);
