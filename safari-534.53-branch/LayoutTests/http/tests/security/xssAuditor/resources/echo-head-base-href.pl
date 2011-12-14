#!/usr/bin/perl -wT
use strict;
use CGI;

my $cgi = new CGI;

if ($cgi->param('enable-full-block')) {
    print "X-XSS-Protection: 1; mode=block\n";
}
print "Content-Type: text/html; charset=UTF-8\n\n";

print "<!DOCTYPE html>\n";
print "<html>\n";
print "<head>\n";
print $cgi->param('q');
print "</head>\n";
print "<body>\n";
print "<script src='safe-script.js'></script>\n";
print "</body>\n";
print "</html>\n";
