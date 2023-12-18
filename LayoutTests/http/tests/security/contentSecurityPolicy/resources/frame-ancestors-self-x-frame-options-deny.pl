#!/usr/bin/perl
use strict;
binmode STDOUT;

print "Content-Type: text/html; charset=UTF-8\n";
print "Content-Security-Policy: frame-ancestors 'self'\n";
print "X-Frame-Options: deny\n\n";

print "<!DOCTYPE html>\n";
print "<html>\n";
print "<body>\n";
print "    <p>This is an IFrame sending a Content Security Policy header containing \"frame-ancestors self\" and a \"X-Frame-Options: deny\" header.</p>\n";
print "</body>\n";
print "</html>\n";
