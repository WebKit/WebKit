#!/usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# This Source Code Form is "Incompatible With Secondary Licenses", as
# defined by the Mozilla Public License, v. 2.0.

use 5.10.1;
use strict;
use warnings;

use File::Basename;
BEGIN { chdir dirname($0); }
use lib qw(. lib);
use Bugzilla;
use Bugzilla::Migrate;

use Getopt::Long;
use Pod::Usage;

my %switch;
GetOptions(\%switch, 'help|h|?', 'from=s', 'verbose|v+', 'dry-run');

# Print the help message if that switch was selected or if --from
# wasn't specified.
if (!$switch{'from'} or $switch{'help'}) {
    pod2usage({-exitval => 1});
}

my $migrator = Bugzilla::Migrate->load($switch{'from'});
$migrator->verbose($switch{'verbose'});
$migrator->dry_run($switch{'dry-run'});
$migrator->check_requirements();
$migrator->do_migration();

# Even if there's an error, we want to be sure that the serial values
# get reset properly.
END {
    if ($migrator and $migrator->dry_run) {
        my $dbh = Bugzilla->dbh;
        if ($dbh->bz_in_transaction) {
            $dbh->bz_rollback_transaction();
        }
        $migrator->reset_serial_values();
    }
}

__END__

=head1 NAME

migrate.pl - A script to migrate from other bug-trackers to Bugzilla.

=head1 SYNOPSIS

 ./migrate.pl --from=<tracker> [--verbose] [--dry-run]

 Migrates from another bug-tracker to Bugzilla. If you want
 to upgrade Bugzilla, use checksetup.pl instead.

 Always test this on a backup copy of your database before
 running it on your live Bugzilla.

=head1 OPTIONS

=over

=item B<--from=tracker>

Specifies what bug-tracker you're migrating from. To see what values
are valid, see the contents of the F<Bugzilla/Migrate/> directory.

=item B<--dry-run>

Don't modify the Bugzilla database at all, just test the import.
Note that this could cause significant slowdown and other strange effects
on a live Bugzilla, so only use it on a test instance.

=item B<--verbose>

If specified, this script will output extra debugging information
to STDERR. Specify multiple times (up to three) for more information.

=back

=head1 DESCRIPTION

This script copies data from another bug-tracker into Bugzilla. It migrates
users, products, and bugs from the other bug-tracker into this Bugzilla,
without removing any of the data currently in this Bugzilla.

Note that you will need enough space in your temporary directory to hold
the size of all attachments in your current bug-tracker.

You may also need to increase the number of file handles a process is allowed
to hold open (as the migrator will create a file handle for each attachment
in your database). On Linux and simliar systems, you can do this as root
by typing C<ulimit -n 65535> before running your script.
