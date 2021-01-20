#!/usr/bin/perl -wT
use strict;

print "Content-Type: text/html\n";
print "Cache-Control: no-cache, no-store\n";
print "X-FRAME-OPTIONS: SAMEORIGIN\n";
print "X-FRAME-OPTIONS: SAMEORIGIN\n\n";

print "<p>This page should load iff it's same origin with it's parent.</p>\n";
