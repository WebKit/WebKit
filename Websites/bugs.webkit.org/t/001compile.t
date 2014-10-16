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
# The Original Code are the Bugzilla Tests.
# 
# The Initial Developer of the Original Code is Zach Lipton
# Portions created by Zach Lipton are Copyright (C) 2001 Zach Lipton.
# All Rights Reserved.
# 
# Contributor(s): Zach Lipton <zach@zachlipton.com>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>


#################
#Bugzilla Test 1#
###Compilation###

use strict;
use 5.008001;
use lib qw(. lib t);
use Config;
use Support::Files;
use Test::More tests => scalar(@Support::Files::testitems);

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
    my $output = `$perl $libs -wc$T $file 2>&1`;
    chomp($output);
    my $return_val = $?;
    $output =~ s/^\Q$file\E syntax OK$//ms;
    diag($output) if $output;
    ok(!$return_val, $file) or diag('--ERROR');
}

my @testitems = @Support::Files::testitems;
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
