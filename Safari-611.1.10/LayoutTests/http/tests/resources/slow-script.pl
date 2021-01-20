#!/usr/bin/perl -w

use strict;
use CGI;
use Time::HiRes qw(sleep);

my $cgi = new CGI;
my $delay = $cgi->param('delay');
$delay = 5000 unless $delay;

# flush the buffers after each print
select (STDOUT);
$| = 1;

print "Content-Type: application/javascript\n";
print "Expires: Thu, 01 Dec 2003 16:00:00 GMT\n";
print "Cache-Control: no-store, no-cache, must-revalidate\n";
print "Pragma: no-cache\n";
print "\n";

sleep $delay / 1000;


