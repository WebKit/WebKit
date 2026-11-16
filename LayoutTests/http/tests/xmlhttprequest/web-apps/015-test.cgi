#!/usr/bin/perl -wT
use strict;
binmode STDOUT;

print "Content-Type: text/plain\nCache-Control: no-store\n\nRESULT:[$ENV{'HTTP_X_TEST_HEADER1'}][$ENV{'HTTP_X_TEST_HEADER2'}][$ENV{'HTTP_X_TEST_HEADER3'}]";
