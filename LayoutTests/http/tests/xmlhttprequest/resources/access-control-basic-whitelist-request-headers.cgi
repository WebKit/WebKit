#!/usr/bin/perl -wT
use strict;

print "Cache-Control: no-store\n";

# This should be a simple request, deny preflight.
if ($ENV{'REQUEST_METHOD'} eq "POST") {
    print "Access-Control-Allow-Credentials: true\n";
    print "Access-Control-Allow-Origin: http://127.0.0.1:8000\n\n";

    print "Accept: $ENV{'HTTP_ACCEPT'}\n";
    print "Accept-Language: $ENV{'HTTP_ACCEPT_LANGUAGE'}\n";
    print "Content-Language: $ENV{'HTTP_CONTENT_LANGUAGE'}\n";
    print "Content-Type: $ENV{'CONTENT_TYPE'}\n";
} else {
    print "\n";
}
