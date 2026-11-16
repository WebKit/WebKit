#!/usr/bin/perl -w

use CGI qw(:standard);
my $cgi = new CGI;

print "Cache-Control: no-cache, no-store\n";
print "Content-type: text/plain\n\n"; 
print "$ENV{\"CONTENT_TYPE\"}\n";
