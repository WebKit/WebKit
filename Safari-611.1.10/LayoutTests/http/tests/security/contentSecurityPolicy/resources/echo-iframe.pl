#!/usr/bin/perl -wT
use strict;
use CGI;

my $cgi = new CGI;

print "Content-Type: text/html; charset=UTF-8\n";
print "Content-Security-Policy: ".$cgi->param('csp')."\n\n";

print "<!DOCTYPE html>\n";
print "<html>\n";
print "<body>\n";
print "<iframe src=\"".$cgi->param('q')."\"></iframe>\n";
print "</body>\n";
print "</html>\n";
