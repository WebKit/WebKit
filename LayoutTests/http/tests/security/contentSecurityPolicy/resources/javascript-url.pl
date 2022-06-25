#!/usr/bin/perl -wT
use strict;
use CGI;

my $cgi = new CGI;

print "Content-Type: text/html; charset=UTF-8\n";
print "Content-Security-Policy: ".$cgi->param('csp')."\n\n";

my $text = "PASS";
$text = "FAIL" if $cgi->param('should_run') eq 'no';

print "<!DOCTYPE html>\n";
print "<html>\n";
print "<body>\n";
print "<iframe src=\"javascript:alert('".$text."');\"></iframe>\n";
print "<object data=\"javascript:alert('".$text."');\"></object>\n";
print "<embed src=\"javascript:alert('".$text."');\"></embed>\n";
print "</html>\n";
