#!/usr/bin/perl -wT
use strict;

print "Content-Type: text/plain\n";
print "Access-Control-Allow-Credentials: true\n";
print "Access-Control-Allow-Origin: http://127.0.0.1:8000\n\n";

print "FAIL: Sandboxed iframe XHR access allowed.\n";
