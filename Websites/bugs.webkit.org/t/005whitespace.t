# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code are the Bugzilla tests.
#
# The Initial Developer of the Original Code is Jacob Steenhagen.
# Portions created by Jacob Steenhagen are
# Copyright (C) 2001 Jacob Steenhagen. All
# Rights Reserved.
#
# Contributor(s): Jacob Steenhagen <jake@bugzilla.org>
#                 David D. Kilzer <ddkilzer@kilzer.net>
#                 Colin Ogilvie <mozilla@colinogilvie.co.uk>
#                 Marc Schumann <wurblzap@gmail.com>
#

#################
#Bugzilla Test 5#
#####no_tabs#####

use strict;

use lib 't';

use Support::Files;
use Support::Templates;

use File::Spec;
use Test::More tests => (  scalar(@Support::Files::testitems)
                         + $Support::Templates::num_actual_files) * 3;

my @testitems = @Support::Files::testitems;
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
