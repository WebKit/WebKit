#!/usr/bin/perl -w

use CGI;
$query = new CGI;

my $cookie = $query->cookie('value');

print "Content-type: text/plain\n";
print "Cache-control: max-age=3600\n";
print "\n";
print "var response = '${cookie}';";
