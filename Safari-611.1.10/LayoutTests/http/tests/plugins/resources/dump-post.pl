#!/usr/bin/perl -w

use CGI;

my $cgi = new CGI;

# Just dump whatever was POSTed to us as text/plain.

print $cgi->header('text/plain');

# Different versions of the CGI module use different conventions for accessing
# the POST data. Because at least one of them will be empty, there is no harm
# in printing both.
print $cgi->param('POSTDATA');
print $cgi->param('keywords');
