#!/usr/bin/perl -wT
use strict;

print "Cache-Control: no-store\n";

# Allow simple requests, but deny preflight.
if ($ENV{'REQUEST_METHOD'} ne "OPTIONS") {
    print "Access-Control-Allow-Credentials: true\n";
    print "Access-Control-Allow-Origin: http://127.0.0.1:8000\n";
}

print "\n";
