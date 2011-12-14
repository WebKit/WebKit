#!/usr/bin/perl -wT
use strict;

print "Content-Type: text/plain\n";
print "Cache-Control: no-store\n";
print 'Cache-Control: no-cache="set-cookie"' . "\n";

my $cookie = $ENV{"HTTP_CLEAR_COOKIE"};

if ($cookie =~ /Max-Age/i) {
    $cookie =~ s/Max-Age *= *[0-9]+/Max-Age=0/i;
} else {
    $cookie .= ";" unless ($cookie =~ m/;$/);
    $cookie .= " " unless ($cookie =~ m/ $/);
    $cookie .= "Max-Age=0";
}

if ($cookie =~ /Expires/i) {
    # Set the "Expires" field to UNIX epoch
    $cookie =~ s/Expires *= *[^;]+/Expires=Thu, 01 Jan 1970 00:00:00 GMT/i;
} else {
    $cookie .= ";" unless ($cookie =~ m/;$/);
    $cookie .= " " unless ($cookie =~ m/ $/);
    $cookie .= "Expires=Thu, 01 Jan 1970 00:00:00 GMT";
}

print "Set-Cookie: $cookie\n\n";
