# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.


##################
#Bugzilla Test 11#
##POD validation##

use 5.10.1;
use strict;
use warnings;

use lib 't';

use Support::Files;
use Pod::Checker;
use Pod::Coverage;

use Test::More tests => scalar(@Support::Files::testitems)
                        + scalar(@Support::Files::module_files);

# These methods do not need to be documented by default.
use constant DEFAULT_WHITELIST => qr/^(?:new|new_from_list|check|run_create_validators)$/;

# These subroutines do not need to be documented, generally because
# you shouldn't call them yourself. No need to include subroutines
# of the form _foo(); they are already treated as private.
use constant SUB_WHITELIST => (
    'Bugzilla::Flag'     => qr/^(?:(force_)?retarget|force_cleanup)$/,
    'Bugzilla::FlagType' => qr/^sqlify_criteria$/,
    'Bugzilla::JobQueue' => qr/(?:^work_once|work_until_done|subprocess_worker)$/,
    'Bugzilla::Search'   => qr/^SPECIAL_PARSING$/,
    'Bugzilla::Template' => qr/^field_name$/,
    'Bugzilla::MIME'     => qr/^as_string$/,
);

# These modules do not need to be documented, generally because they
# are subclasses of another module which already has all the relevant
# documentation. Partial names are allowed.
use constant MODULE_WHITELIST => qw(
    Bugzilla::Auth::Login::
    Bugzilla::Auth::Persist::
    Bugzilla::Auth::Verify::
    Bugzilla::BugUrl::
    Bugzilla::Config::
    Bugzilla::Extension::
    Bugzilla::Job::
    Bugzilla::Migrate::
    docs::lib::Pod::Simple::
);

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

my @testitems = @Support::Files::testitems;

foreach my $file (@testitems) {
    $file =~ s/\s.*$//; # nuke everything after the first space (#comment)
    next if (!$file); # skip null entries
    my $error_count = podchecker($file, $fh);
    if ($error_count < 0) {
        ok(1,"$file does not contain any POD");
    } elsif ($error_count == 0) {
        ok(1,"$file has correct POD syntax");
    } else {
        ok(0,"$file has incorrect POD syntax --ERROR");
    }
}

my %sub_whitelist = SUB_WHITELIST;
my @module_files = sort @Support::Files::module_files;

foreach my $file (@module_files) {
    my $module = $file;
    $module =~ s/\.pm$//;
    $module =~ s#/#::#g;
    $module =~ s/^extensions/Bugzilla::Extension/;

    my @whitelist = (DEFAULT_WHITELIST);
    push(@whitelist, $sub_whitelist{$module}) if $sub_whitelist{$module};

    # XXX Once all methods are correctly documented, nonwhitespace should
    # be set to 1.
    my $cover = Pod::Coverage->new(package => $module, nonwhitespace => 0,
                                   trustme => \@whitelist);
    my $coverage = $cover->coverage;
    my $reason = $cover->why_unrated;

    if (defined $coverage) {
        if ($coverage == 1) {
            ok(1, "$file has POD for all methods");
        }
        else {
            ok(0, "$file POD coverage is " . sprintf("%u%%", 100 * $coverage) .
                  ". Undocumented methods: " . join(', ', $cover->uncovered));
        }
    }
    # These errors are thrown when the module couldn't be loaded due to
    # a missing dependency.
    elsif ($reason =~ /^(?:no public symbols defined|requiring '[^']+' failed)$/) {
        ok(1, "$file cannot be loaded");
    }
    elsif ($reason eq "couldn't find pod") {
        if (grep { $module =~ /^\Q$_\E/ } MODULE_WHITELIST) {
            ok(1, "$file does not contain any POD (whitelisted)");
        }
        else {
            ok(0, "$file POD coverage is 0%");
        }
    }
    else {
        ok(0, "$file: $reason");
    }
}

exit 0;
