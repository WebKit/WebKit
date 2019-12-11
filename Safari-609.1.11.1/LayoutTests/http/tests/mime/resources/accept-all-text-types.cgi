#!/usr/bin/perl -wT
use strict;

my $type = $ENV{'QUERY_STRING'};
$type =~ s/type=//;

print "Content-Type: text/$type\n\n";
print "PASS\n";
