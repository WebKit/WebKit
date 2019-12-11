#!/usr/bin/perl -wT
use strict;
use CGI;

my $cgi = new CGI;

print "Content-Type: text/html; charset=UTF-8\n";
print "Content-Security-Policy: ".$cgi->param('csp')."\n\n";

my ($text, $replacement) = ("FAIL", "PASS");
($text, $replacement) = ($replacement, $text) if $cgi->param('should_run') eq 'no';

print "<!DOCTYPE html>\n";
print "<html>\n";
print "<body onload='var result = document.getElementById(\"result\"); result.firstChild.nodeValue = result.attributes.getNamedItem(\"text\").value;'>\n";
print "<div id=\"result\" text=\"$replacement\">\n";
print "$text\n";
print "</div>\n";
print "</body>\n";
print "</html>\n";
