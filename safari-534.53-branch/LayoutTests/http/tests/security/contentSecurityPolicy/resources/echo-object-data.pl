#!/usr/bin/perl -wT
use strict;
use CGI;

my $cgi = new CGI;

print "Content-Type: text/html; charset=UTF-8\n";
print "X-WebKit-CSP: ".$cgi->param('csp')."\n\n";

print "<!DOCTYPE html>\n";
print "<html>\n";
print "<body>\n";
print "<object data=\"".$cgi->param('q')."\"></object>\n";
print "</body>\n";
print "</html>\n";
