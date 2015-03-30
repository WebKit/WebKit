#!/usr/bin/perl -w

use CGI;
use HTTP::Date;

my $query = new CGI;
@names = $query->param;
my $includeBody = $query->param('include-body') || 0;
my $expiresInFutureIn304 = $query->param('expires-in-future-in-304') || 0;

my $hasStatusCode = 0;
my $hasExpiresHeader = 0;
if ($query->http && $query->http("If-None-Match") eq "match") {
    print "Status: 304\n";
    $hasStatusCode = 1;
    if ($expiresInFutureIn304) {
        print "Expires: " . HTTP::Date::time2str(time() + 100) . "\n";
        $hasExpiresHeader = 1;
    }
}

foreach (@names) {
    next if ($_ eq "uniqueId");
    next if ($_ eq "include-body");
    next if ($_ eq "Status" and $hasStatusCode);
    next if ($_ eq "Expires" and $hasExpiresHeader);
    print $_ . ": " . $query->param($_) . "\n";
}
print "\n";
print "test" if $includeBody;
