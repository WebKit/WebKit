# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.


#################
#Bugzilla Test 6#
####Spelling#####

use 5.10.1;
use strict;
use warnings;

use lib 't';
use Support::Files;

# -1 because 006spellcheck.t must not be checked.
use Test::More tests => scalar(@Support::Files::testitems)
                        + scalar(@Support::Files::test_files) - 1;

# Capture the TESTOUT from Test::More or Test::Builder for printing errors.
# This will handle verbosity for us automatically.
my $fh;
{
    no warnings qw(unopened);  # Don't complain about non-existent filehandles
    if (-e \*Test::More::TESTOUT) {
        $fh = \*Test::More::TESTOUT;
    } elsif (-e \*Test::Builder::TESTOUT) {
        $fh = \*Test::Builder::TESTOUT;
    } else {
        $fh = \*STDOUT;
    }
}

my @testitems = (@Support::Files::testitems, @Support::Files::test_files);

#add the words to check here:
my @evilwords = qw(
    anyways
    appearence
    arbitary
    cancelled
    critera
    databasa
    dependan
    existance
    existant
    paramater
    refered
    repsentation
    suported
    varsion
);

my $evilwordsregexp = join('|', @evilwords);

foreach my $file (@testitems) {
    $file =~ s/\s.*$//; # nuke everything after the first space (#comment)
    next if (!$file); # skip null entries
    # Do not try to validate this file as it obviously contains a list
    # of wrongly spelled words.
    next if ($file eq 't/006spellcheck.t');

    if (open (FILE, $file)) { # open the file for reading

        my $found_word = '';

        while (my $file_line = <FILE>) { # and go through the file line by line
            if ($file_line =~ /($evilwordsregexp)/i) { # found an evil word
                $found_word = $1;
                last;
            }
        }
            
        close (FILE);
            
        if ($found_word) {
            ok(0,"$file: found SPELLING ERROR $found_word --WARNING");
        } else {
            ok(1,"$file does not contain registered spelling errors");
        }
    } else {
        ok(0,"could not open $file for spellcheck --WARNING");
    }
} 

exit 0;
