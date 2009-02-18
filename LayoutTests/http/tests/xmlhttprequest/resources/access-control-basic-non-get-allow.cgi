#!/usr/bin/perl -wT
use strict;

my $request;

if ($ENV{'REQUEST_METHOD'} eq "OPTIONS") {
    print "Content-Type: text/plain\n";
    print "Access-Control-Allow-Credentials: true\n";
    print "Access-Control-Allow-Methods: PUT\n";
    print "Access-Control-Allow-Origin: http://127.0.0.1:8000\n\n";
} elsif ($ENV{'REQUEST_METHOD'} eq "PUT") {
    print "Content-Type: text/plain\n";
    print "Access-Control-Allow-Credentials: true\n";
    print "Access-Control-Allow-Origin: http://127.0.0.1:8000\n\n";

    print "PASS: Cross-domain access allowed.\n";
    read(STDIN, $request, $ENV{'CONTENT_LENGTH'}) || die "Could not read in content.\n";
    print $request;
} else {
    print "Wrong method: " . $ENV{'REQUEST_METHOD'} . "\n";
}
