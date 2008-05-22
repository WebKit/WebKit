#!/usr/bin/perl -wT
use strict;

print "Content-Type: text/plain\n";
print "Access-Control: allow <http://127.0.0.1:8000> exclude <http://127.0.0.1:8000>\n\n";

print "FAIL: Cross-domain access allowed.\n";
