#!/usr/bin/perl -w

print "Content-type: application/octet-stream\n";
print "Cache-control: no-cache, no-store\n\n";
sleep(1);
open FH, "<../../../resources/Ahem.ttf" or die;
while (<FH>) { print; }
close FH;
