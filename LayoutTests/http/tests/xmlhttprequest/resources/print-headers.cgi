#!/usr/bin/perl -wT
use strict;
binmode STDOUT;

print "Content-Type: text/plain\n";
print "Cache-Control: no-store\n\n";

foreach (keys %ENV) {
    if ($_ =~ "HTTP_") {
        print $_ . ": " . $ENV{$_} . "\n";
    }
}
