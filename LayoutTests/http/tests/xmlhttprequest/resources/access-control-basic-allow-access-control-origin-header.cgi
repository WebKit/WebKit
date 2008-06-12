#!/usr/bin/perl -wT
use strict;

print "Content-Type: text/plain\n";
print "Cache-Control: no-cache, no-store\n";
print "Access-Control: allow <*>\n\n";

print "PASS: Cross-domain access allowed.\n";
print "HTTP_ACCESS_CONTROL_ORIGIN: " . $ENV{"HTTP_ACCESS_CONTROL_ORIGIN"} . "\n";
