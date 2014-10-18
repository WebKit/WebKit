#!/usr/bin/perl -w

use CGI;
use HTTP::Date;

print "Content-type: text/plain\n";
print "\n";

my $file = "../../css/resources/abe.png";

open (FH,'<', $file) || die "Could not open $file: $!";
my $buffer = "";
while (read(FH, $buffer, 10240)) {
    print $buffer;
}
close(FH);
