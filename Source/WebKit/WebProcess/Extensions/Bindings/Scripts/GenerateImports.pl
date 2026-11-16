#!/usr/bin/env perl
use strict;
use warnings;

use Getopt::Long;

my $output_file;
my $generate_cpp;

die "Usage: $0 --cpp=true --output=output_file -- import1 import2 ... importN\n" if @ARGV < 2;

GetOptions('cpp' => \$generate_cpp,
           'output=s' => \$output_file);

my @import_files = @ARGV;

open(my $fh, '>', $output_file) or die "Cannot open $output_file: $!";

my $import_macro = $generate_cpp ? "#include" : "#import";

foreach my $file (@import_files) {
    print $fh "$import_macro \"$file\"\n";
}

close($fh);
