#!/usr/bin/perl -w

use CGI qw(:standard);
my $cgi = new CGI;

print "Cache-Control: no-cache, no-store\n";
print "Content-type: text/plain\n\n"; 
print "x-test: $ENV{\"HTTP_X_TEST\"}\n";
