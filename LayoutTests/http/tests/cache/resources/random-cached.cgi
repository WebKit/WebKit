#!/usr/bin/perl -w

print "Content-type: text/javascript\n";
print "Cache-control: max-age=60000\n";
print "ETag: \"123456789\"\n";
print "\n";

my $random_number = int(rand(1000000000000));
print "top.randomNumber = " . $random_number . ";\n";
print "top.document.querySelector('h1').textContent = top.randomNumber;";
