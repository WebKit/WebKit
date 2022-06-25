# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

#################
#Bugzilla Test 9#
####bugwords#####

# Bugzilla has a mechanism for taking various words, including "bug", "bugs", 
# and "a bug" and automatically replacing them in the templates with the local
# terminology. It does this by using the 'terms' hash, so "bug" becomes 
# "[% terms.bug %]". This test makes sure the relevant words aren't used
# bare.

use 5.10.1;
use strict;
use warnings;

use lib 't';

use Support::Files;
use Support::Templates;
use Bugzilla::Util;

use File::Spec;

use Test::More tests => ($Support::Templates::num_actual_files); 

# Find all the templates
my @testitems;
for my $path (@Support::Templates::include_paths) {
    push(@testitems, map(File::Spec->catfile($path, $_),
                         Support::Templates::find_actual_files($path)));
}

foreach my $file (@testitems) {
    my @errors;
    
    # Read the entire file into a string
    local $/;
    open (FILE, "<$file") || die "Can't open $file: $!\n";    
    my $slurp = <FILE>;
    close (FILE);

    # /g means we execute this loop for every match
    # /s means we ignore linefeeds in the regexp matches
    # This extracts everything which is _not_ a directive.
    while ($slurp =~ /%\](.*?)(\[%|$)/gs) {
        my $text = $1;

        my @lineno = ($` =~ m/\n/gs);
        my $lineno = scalar(@lineno) + 1;
    
        # "a bug", "bug", "bugs"
        if (grep /(a?[\s>]bugs?[\s.:;,<])/i, $text) {
            # Exclude variable assignment.
            unless (grep /bugs =/, $text) {
                push(@errors, [$lineno, $text]);
                next;
            }
        }
    }
        
    if (scalar(@errors)) {
      ok(0, "$file contains invalid bare words (e.g. 'bug') --WARNING");
      
      foreach my $error (@errors) {
        print "$error->[0]: $error->[1]\n";
      }
    } 
    else {
      ok(1, "$file has no invalid barewords");
    }
}

exit 0;
