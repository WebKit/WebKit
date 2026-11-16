#!/usr/bin/perl -wT
use strict;
binmode STDOUT;

print "Content-Type: text/plain\n\n";

print "FAIL: Sandboxed iframe XHR access allowed.\n";
