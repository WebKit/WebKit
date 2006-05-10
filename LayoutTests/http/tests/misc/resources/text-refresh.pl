#!/usr/bin/perl
# Simple script to generate a redirect to a success document.

print "Content-type: text/plain\r\n";
print "Refresh: 0; URL=200.html\r\n";
print "\r\n";

print "FAILURE\r\n";
