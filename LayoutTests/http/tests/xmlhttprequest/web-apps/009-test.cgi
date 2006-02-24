#!/usr/bin/perl -wT
use strict;

$| = 1;
print "Content-Type: text/xml\nCache-Control: no-store\n\n<test>";
sleep 60;
print "</test>\n";
