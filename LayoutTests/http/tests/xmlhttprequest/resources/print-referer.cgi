#!/usr/bin/perl -w

use CGI qw(:standard);
my $cgi = new CGI;

print "Content-type: text/plain\n\n"; 
if ($cgi->referer) {
  print $cgi->referer;
} else {
  print "NO REFERER";
}
