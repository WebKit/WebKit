#!/usr/bin/perl -wT
use strict;

print "Content-Type: text/plain\n";
print "Cache-Control: no-store\n\n";

foreach (keys %ENV) {
    if ($_ =~ "HTTP_") {
        print $_ . ": " . $ENV{$_} . "\n";
    }
}
