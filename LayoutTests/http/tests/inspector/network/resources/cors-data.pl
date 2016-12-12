#!/usr/bin/perl -wT
use strict;

print "Content-Type: application/json\n";
print "Access-Control-Allow-Credentials: true\n";
print "Access-Control-Allow-Origin: http://127.0.0.1:8000\n";
print "X-Custom-Header: Custom-Header-Value\n\n";

print "{\"json\": true, \"value\": 42}";
