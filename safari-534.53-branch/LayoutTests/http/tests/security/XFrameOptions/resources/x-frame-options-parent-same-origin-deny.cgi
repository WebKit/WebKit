#!/usr/bin/perl -wT
use strict;

print "Content-Type: text/html\n";
print "Cache-Control: no-cache, no-store\n";
print "X-FRAME-OPTIONS: sameorigin\n\n";

print "<p>FAIL: This should not show up as the parent is not in the same origin.</p>\n";
