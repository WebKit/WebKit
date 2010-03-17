#!/usr/bin/perl -w

# flush the buffers after each print
select (STDOUT);
$| = 1;

print "Content-Type: application/x-webkit-test-netscape\n";
print "\n";
print "\x43\n\n";
