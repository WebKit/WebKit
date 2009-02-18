#!/usr/bin/perl -wT
use strict;

# in whitelist
print "content-type: text/plain\n";
print "cache-control: no cache\n";
print "content-language: en\n";
print "expires: Fri, 30 Oct 1998 14:19:41 GMT\n";
print "last-modified: Tue, 15 Nov 1994 12:45:26 GMT\n";
print "pragma: no-cache\n";

# not in whitelist
print "x-webkit: foobar\n";

print "Access-Control-Allow-Origin: *\n\n";

print "PASS: Cross-domain access allowed.\n";
