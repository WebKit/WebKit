#!/usr/bin/perl -w

use CGI qw(:standard);
my $cgi = new CGI;

print "Content-type: text/plain\n\n"; 
print $ENV{"QUERY_STRING"};
