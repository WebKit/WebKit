#!/usr/bin/perl -wT
use strict;
use CGI;

my $cgi = new CGI;

print "Content-Type: text/html; charset=UTF-8\n\n";

print "<!DOCTYPE html>\n";
print "<html>\n";
print "<body foo=\"";
print $cgi->param('q');
if ($cgi->param('clutter')) {
    print $cgi->param('clutter');
}
print "\">\n";
print "<script>var y = 123;</script>";
print "</body>\n";
print "</html>\n";
