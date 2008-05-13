#!/usr/bin/perl -w

use CGI;

my $cgi = new CGI;

# Just dump whatever was POSTed to us as text/plain.

print $cgi->header('text/plain');
print $cgi->param('keywords');
