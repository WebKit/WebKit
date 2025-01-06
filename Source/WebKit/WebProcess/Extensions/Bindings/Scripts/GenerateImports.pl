#!/usr/bin/env perl
use strict;
use warnings;

die "Usage: $0 output_file import1 import2 ... importN\n" if @ARGV < 2;

my $output_file = shift @ARGV;
my @import_files = @ARGV;

open(my $fh, '>', $output_file) or die "Cannot open $output_file: $!";

foreach my $file (@import_files) {
    print $fh "#import \"$file\"\n";
}

close($fh);

print "Generated $output_file with #import statements for provided files.\n";
