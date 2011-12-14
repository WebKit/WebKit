#!/usr/bin/perl -wT
use strict;

print "Content-Type: text/plain\n";
print "Cache-Control: no-store\n";
print 'Cache-Control: no-cache="set-cookie"' . "\n";

# We only map the SET_COOKIE request header to "Set-Cookie"
print "Set-Cookie: " . $ENV{"HTTP_SET_COOKIE"} . "\n\n";
