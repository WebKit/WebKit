#!/usr/bin/perl -wT
use strict;

$| = 1;
print "Content-Type: text/plain\nCache-Control: no-store\n\nTOP (wait 60 seconds for the next line)\n";
sleep 10;
print "BOTTOM\n";
