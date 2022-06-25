#!/usr/bin/perl -wT
use strict;

print "Content-Type: text/plain\n";
print "Cache-Control: no-cache, no-store\n";
print "Access-Control-Allow-Origin: *\n\n";

print "PASS: Cross-domain access allowed.\n";
print "HTTP_ORIGIN: " ;
print $ENV{"HTTP_ORIGIN"} if defined $ENV{"HTTP_ORIGIN"};
print "\n";
