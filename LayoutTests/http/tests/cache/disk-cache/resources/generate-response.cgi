#!/usr/bin/perl -w

use CGI;
use HTTP::Date;

my $query = new CGI;
@names = $query->param;

if ($query->http && $query->http("If-None-Match") eq "match") {
    print "Status: 304\n";
}

foreach (@names) {
    next if ($_ eq "uniqueId");
    print $_ . ": " . $query->param($_) . "\n";
}
print "\n";
print "test";
