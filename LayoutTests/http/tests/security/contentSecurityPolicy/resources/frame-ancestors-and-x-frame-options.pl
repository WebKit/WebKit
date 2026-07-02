#!/usr/bin/perl
use strict;
use CGI;

my $cgi = new CGI;

print "Content-Type: text/html; charset=UTF-8\n";
print "Content-Security-Policy: frame-ancestors " . $cgi->param("policy") . "\n";
print "X-Frame-Options: " . $cgi->param("xfo") . "\n\n";

print "<!DOCTYPE html>\n";
print "<html>\n";
print "<body>\n";
print "    <p>This is an IFrame sending a Content Security Policy header containing \"frame-ancestors " . $cgi->param("policy") . "\" and \"X-Frame-Options: " . $cgi->param("xfo") . "\".</p>\n";
print "</body>\n";
print "</html>\n";
