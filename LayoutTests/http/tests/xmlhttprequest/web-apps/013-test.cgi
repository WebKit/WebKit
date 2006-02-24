#!/usr/bin/perl -wT
use strict;

sleep 3;

$| = 1;
print "Status: 400 Good work\nContent-Type: text/plain\nCache-Control: no-store\n\nBODY";
