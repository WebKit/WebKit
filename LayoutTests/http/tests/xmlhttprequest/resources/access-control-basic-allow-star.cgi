#!/usr/bin/perl -wT
use strict;
binmode STDOUT;

print "Content-Type: text/plain\n";
print "Access-Control-Allow-Origin: *\n\n";

print "PASS: Cross-domain access allowed.\n";
