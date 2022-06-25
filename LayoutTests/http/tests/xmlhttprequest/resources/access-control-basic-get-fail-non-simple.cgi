#!/usr/bin/perl -wT
use strict;

my $request;

if ($ENV{'REQUEST_METHOD'} eq "GET") {
    print "Content-Type: text/plain\n";
    print "Access-Control-Allow-Credentials: true\n";
    print "Access-Control-Allow-Origin: http://127.0.0.1:8000\n\n";
    print "FAIL: Cross-domain access allowed.\n";
} else {
    # Generate internal server error politely.
    print "Status: 500 Internal Server Error\n\n";
}
