#!/usr/bin/perl -w

use CGI;
use HTTP::Date;

print "Content-type: text/javascript\n"; 

my $query = new CGI;
@names = $query->param;
foreach (@names) {
    next if ($_ eq "uniqueId");
    print $_ . ": " . $query->param($_) . "\n";
}

my $random_number = int(rand(1000000000000));
# include randam etag to stop apache from doing any caching
print "ETag: " . $random_number . "\n";
print "\n";

print "randomNumber = " . $random_number . ";\n";
