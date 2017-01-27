#!/usr/bin/perl -w

use CGI;

print "Content-type: text/html\n";
print "Cache-control: no-cache\n";
print "\n";

my $query = new CGI;
my $scriptCacheControl = $query->param("script-cache-control");

print "<script src='cache-simulator.cgi?uniqueId=1&Cache-control=";
print $scriptCacheControl;
print "'></script>\n";
