#!/usr/bin/perl -wT
use strict;


print "Content-Type: application/x-webkit-test-netscape\n";
print "Content-Security-Policy: object-src 'none'\n";
print "\n";

print "This is a mock plugin. It does pretty much nothing.";
