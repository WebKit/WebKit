# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

#################
#Bugzilla Test 5#
#####no_tabs#####

use 5.10.1;
use strict;
use warnings;

use lib 't';

use Support::Files;
use Support::Templates;

use File::Spec;
use Test::More tests => (scalar(@Support::Files::testitems)
                         + scalar(@Support::Files::test_files)
                         + $Support::Templates::num_actual_files) * 3;

my @testitems = (@Support::Files::testitems, @Support::Files::test_files);
for my $path (@Support::Templates::include_paths) {
   push(@testitems, map(File::Spec->catfile($path, $_),
                        Support::Templates::find_actual_files($path)));
}

my %results;

foreach my $file (@testitems) {
    open (FILE, "$file");
    my @contents = <FILE>;
    if (grep /\t/, @contents) {
        ok(0, "$file contains tabs --WARNING");
    } else {
        ok(1, "$file has no tabs");
    }
    close (FILE);
}

foreach my $file (@testitems) {
    open (FILE, "$file");
    my @contents = <FILE>;
    if (grep /\r/, @contents) {
        ok(0, "$file contains non-OS-conformant line endings --WARNING");
    } else {
        ok(1, "All line endings of $file are OS conformant");
    }
    close (FILE);
}

foreach my $file (@testitems) {
    open (FILE, "$file");
    my $first_line = <FILE>;
    if ($first_line =~ /\xef\xbb\xbf/) {
        ok(0, "$file contains Byte Order Mark --WARNING");
    } else {
        ok(1, "$file is free of a Byte Order Mark");
    }
    close (FILE);
}

exit 0;
