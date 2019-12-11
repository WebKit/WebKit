#!/usr/bin/perl -wT
use strict;

print "Content-Type: text/html\n";
print "Cache-Control: no-cache, no-store\n";
print "X-FRAME-OPTIONS: INVALID INVALID INVALID\n\n";

print "<p>PASS: This text should show up.</p>\n";
