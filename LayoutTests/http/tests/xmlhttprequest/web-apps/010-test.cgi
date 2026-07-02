#!/usr/bin/perl -wT
use strict;
binmode STDOUT;

$| = 1;
print "Content-Type: text/xml\nCache-Control: no-store\n\n<test></test>";
