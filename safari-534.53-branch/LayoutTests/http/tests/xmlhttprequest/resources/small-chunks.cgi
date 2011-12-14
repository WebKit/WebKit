#!/usr/bin/perl

use IO::Socket;

$| = 1;

autoflush STDOUT 1;

print "Content-Type: text/html; charset=ISO-8859-1\n\n";

for (my $i = 0; $i < 5; $i++) {
    print "This is chunk number $i";
    sleep 1;
}
