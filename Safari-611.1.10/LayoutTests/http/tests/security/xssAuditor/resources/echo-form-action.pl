#!/usr/bin/perl -wT
use strict;
use CGI;

my $cgi = new CGI;

print "Content-Type: text/html; charset=UTF-8\n\n";

print "<!DOCTYPE html>\n";
print "<html>\n";
print "<body>\n";
print "<p>This is an iframe with a injected form</p>\n";
print "<form method=\"post\" id=\"login\" action=\"".$cgi->param('q')."\"></form>\n";
print "<script>if (window.testRunner) testRunner.notifyDone();</script>\n";
print "</body>\n";
print "</html>\n";
