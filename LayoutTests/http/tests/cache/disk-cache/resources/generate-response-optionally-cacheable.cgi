#!/usr/bin/perl -w

use CGI;
use HTTP::Date;

my $query = new CGI;

print "Status: 200\n";
if ($query->http && $query->http("X-Cacheable") eq "true") {
    print "Expires: " . HTTP::Date::time2str(time() + 100) . "\n";
} else {
    print "Cache-Control: no-store\n";
}

print "\n";
