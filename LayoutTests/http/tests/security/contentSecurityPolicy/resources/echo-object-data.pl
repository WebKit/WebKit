#!/usr/bin/perl -wT
use strict;
use CGI;

my $cgi = new CGI;

print "Content-Type: text/html; charset=UTF-8\n";
print "X-WebKit-CSP: ".$cgi->param('csp')."\n\n";

print "<!DOCTYPE html>\n";
print "<html>\n";
print "<body>\n";
print "<script src=\"/plugins/resources/mock-plugin-logger.js\"></script>\n";
print "<object data=\"".$cgi->param('plugin')."\"\n";
print "        log=\"".$cgi->param('log')."\"\n" if $cgi->param('log');
print "        type=\"".$cgi->param('type')."\"\n" if $cgi->param('type');
print "></object>\n";
print "</body>\n";
print "</html>\n";
