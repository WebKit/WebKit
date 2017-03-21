# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.


#################
#Bugzilla Test 1#
###Compilation###

use 5.10.1;
use strict;
use warnings;

use lib qw(. lib t);
use Config;
use Support::Files;
use Test::More tests => scalar(@Support::Files::testitems)
                        + scalar(@Support::Files::test_files);

BEGIN { 
    use_ok('Bugzilla::Constants');
    use_ok('Bugzilla::Install::Requirements');
    use_ok('Bugzilla');
}

sub compile_file {
    my ($file) = @_;

    # Don't allow CPAN.pm to modify the global @INC, which the version
    # shipped with Perl 5.8.8 does. (It gets loaded by 
    # Bugzilla::Install::CPAN.)
    local @INC = @INC;

    if ($file =~ s/\.pm$//) {
        $file =~ s{/}{::}g;
        use_ok($file);
        return;
    }

    open(my $fh, $file);
    my $bang = <$fh>;
    close $fh;

    my $T = "";
    if ($bang =~ m/#!\S*perl\s+-.*T/) {
        $T = "T";
    }

    my $libs = '';
    if ($ENV{PERL5LIB}) {
       $libs = join " ", map { "-I\"$_\"" } split /$Config{path_sep}/, $ENV{PERL5LIB};
    }
    my $perl = qq{"$^X"};
    my $output = `$perl $libs -c$T $file 2>&1`;
    chomp($output);
    my $return_val = $?;
    $output =~ s/^\Q$file\E syntax OK$//ms;
    diag($output) if $output;
    ok(!$return_val, $file) or diag('--ERROR');
}

my @testitems = (@Support::Files::testitems, @Support::Files::test_files);
my $file_features = map_files_to_features();

# Test the scripts by compiling them
foreach my $file (@testitems) {
    # These were already compiled, above.
    next if ($file eq 'Bugzilla.pm' 
             or $file eq 'Bugzilla/Constants.pm'
             or $file eq 'Bugzilla/Install/Requirements.pm');
    SKIP: {
        if ($file eq 'mod_perl.pl') {
            skip 'mod_perl.pl cannot be compiled from the command line', 1;
        }
        my $feature = $file_features->{$file};
        if ($feature and !Bugzilla->feature($feature)) {
            skip "$file: $feature not enabled", 1;
        }

        # Check that we have a DBI module to support the DB, if this 
        # is a database module (but not Schema)
        if ($file =~ m{Bugzilla/DB/([^/]+)\.pm$}
            and $file ne "Bugzilla/DB/Schema.pm") 
        {
            my $module = lc($1);
            my $dbd = DB_MODULE->{$module}->{dbd}->{module};
            eval("use $dbd; 1") or skip "$file: $dbd not installed", 1;
        }

        compile_file($file);
    }
}      
