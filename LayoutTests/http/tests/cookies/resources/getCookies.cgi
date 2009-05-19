#!/usr/bin/perl -wT
use strict;

print "Content-Type: text/plain\n";
print "Cache-Control: no-store\n";
print 'Cache-Control: no-cache="set-cookie"' . "\n";

print "HTTP_COOKIE: " . ($ENV{HTTP_COOKIE} || "") . "\n\n";
