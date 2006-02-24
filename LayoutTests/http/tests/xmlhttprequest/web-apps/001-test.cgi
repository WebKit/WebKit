#!/usr/bin/perl -wT
use strict;

if ($ENV{'HTTP_X_TEST_HEADER'} eq 'Test') {
    print "Content-Type: text/plain\nCache-Control: no-store\n\nPASS";
} else {
    print "Content-Type: text/plain\nCache-Control: no-store\n\nFAIL";
}
