#!/usr/bin/perl -wT
use strict;
binmode STDOUT;

print "Content-Type: text/plain\n";
# Support CORS preflight.
print "Access-Control-Allow-Origin: " . ( defined($ENV{"HTTP_ORIGIN"}) ? $ENV{"HTTP_ORIGIN"} : "*" ) . "\n";
print "Access-Control-Allow-Headers: X-SET-COOKIE\n";
print "Access-Control-Allow-Credentials: true\n" if defined($ENV{"HTTP_ORIGIN"});
print "Cache-Control: no-store\n";
print 'Cache-Control: no-cache="set-cookie"' . "\n";

# We only map the X-SET_COOKIE request header to "Set-Cookie", but not on the CORS preflight.
print "Set-Cookie: " . $ENV{"HTTP_X_SET_COOKIE"} if $ENV{"HTTP_X_SET_COOKIE"};
print "\n\n";
