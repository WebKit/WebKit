#!/usr/bin/perl -w

print "Content-type: text/plain\n"; 
print "Content-Length: 0\n";
my $cl = $ENV{'CONTENT_LENGTH'};
my $ct = $ENV{'CONTENT_TYPE'};
my $method = $ENV{'REQUEST_METHOD'};
if (defined $cl) {
    print "REQLENGTH: $cl\n";
}
if (defined $ct) {
    print "REQTYPE: $ct\n";
}
print "REQMETHOD: $method\n";

print "\n";
