#!/usr/bin/perl -w

print "Content-type: text/javascript\n";
print "Cache-control: no-store\n";
print "\n";

my $random_number = int(rand(1000000000000));
print "randomNumber = " . $random_number . ";\n";
print "document.querySelector('h1').textContent = randomNumber;";
