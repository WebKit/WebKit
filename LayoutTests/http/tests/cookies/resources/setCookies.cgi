#!/usr/bin/perl -wT
use strict;
binmode STDOUT;

print "Content-Type: text/plain\n";
print "Access-Control-Allow-Origin: " . $ENV{"HTTP_ORIGIN"} . "\n";
print "Access-Control-Allow-Headers: X-SET-COOKIE\n";
print "Access-Control-Allow-Credentials: true\n";
print "Cache-Control: no-store\n";
print 'Cache-Control: no-cache="set-cookie"' . "\n";

# We only map the X-SET_COOKIE request header to "Set-Cookie"
print "Set-Cookie: " . $ENV{"HTTP_X_SET_COOKIE"} . "\n\n";
