#!/usr/bin/perl -wT
use strict;

my $val1 = exists($ENV{'HTTP_X_TEST_HEADER1'}) ? $ENV{'HTTP_X_TEST_HEADER1'} : "";
my $val2 = exists($ENV{'HTTP_X_TEST_HEADER2'}) ? $ENV{'HTTP_X_TEST_HEADER2'} : "";

print "Content-Type: text/plain\nCache-Control: no-store\n\nRESULT:[${val1}][${val2}]";
