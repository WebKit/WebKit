#!/usr/bin/perl -w
binmode STDIN;
binmode STDOUT;

print "Content-type: text/plain\n";
print "Cache-control: max-age=3600\n";
print "Age: 0\n";
print "\n";
read(STDIN, $data, $ENV{'CONTENT_LENGTH'});
print $data
