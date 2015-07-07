#!/usr/bin/perl -wT
use strict;

print "Content-Type: text/html\n";
print "Cache-Control: no-cache, no-store\n";
print "X-FRAME-OPTIONS: ALLOWALL\n";
print "X-FRAME-OPTIONS: DENY\n\n";

print "<p>FAIL: This page should be blocked.</p>\n";
