#!/usr/bin/perl
# Simple script to generate a 304 HTTP status
binmode STDOUT;

print "Status: 304 Not Modified\r\n";
print "\r\n";
