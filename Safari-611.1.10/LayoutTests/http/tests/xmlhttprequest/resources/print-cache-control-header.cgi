#!/usr/bin/perl -wT
use strict;

print "Content-Type: text/plain\n\n";

foreach (keys %ENV) {
    if ($_ =~ "HTTP_CACHE_CONTROL") {
        print $_ . ": " . $ENV{$_} . "\n";
    }
}
