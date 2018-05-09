#!/usr/bin/env perl

# Copyright (C) 2018 Bocoup LLC. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above
#    copyright notice, this list of conditions and the following
#    disclaimer.
# 2. Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials
#    provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

use strict;
use warnings;
package Test262::Import;

use Cwd qw/abs_path/;
use File::Path qw/rmtree/;
use File::Temp qw/tempdir/;
use FindBin;
use Getopt::Long qw/GetOptions/;
use Pod::Usage;
use Env qw(T262_EXEC_BIN);

my $Bin = $T262_EXEC_BIN || $FindBin::Bin;

my $test262Dir = abs_path("$Bin/../../../JSTests/test262");
my $revisionFile = abs_path("$Bin/../../../JSTests/test262/test262-Revision.txt");
my $summaryFile = abs_path("$Bin/../../../JSTests/test262/latest-changes-summary.txt");
my $sourceDir;
my $remoteUrl;
my $branch;

main();

sub processCLI {
    my $help = 0;

    GetOptions(
        's|src=s' => \$sourceDir,
        'r|remote=s' => \$remoteUrl,
        'b|branch=s' => \$branch,
        'h|help' => \$help,
    );

    if ($help) {
        pod2usage(-exitstatus => 0, -verbose => 1);
    }

    if ($sourceDir and $remoteUrl) {
        print "Please specify only a single location for the Test262 repository.\n\n";
        pod2usage(-exitstatus => 2, -verbose => 1);
    };

    if ($sourceDir) {
        if (not -d $sourceDir
            or not -d "$sourceDir/.git"
            or not -d "$sourceDir/test"
            or not -d "$sourceDir/harness") {
            print "$sourceDir does not exist or is not a valid Test262 folder.\n\n";
            pod2usage(-exitstatus => 3, -verbose => 1);
        };

        if (abs_path($sourceDir) eq $test262Dir) {
            print "$sourceDir cannot be the same as the current Test262 folder.\n\n";
            pod2usage(-exitstatus => 4, -verbose => 1);
        }
    } else {
        $remoteUrl ||= 'git@github.com:tc39/test262.git';
        $branch ||= 'master';
    }

    print "Settings:\n";
    print "Source: $sourceDir\n" if $sourceDir;
    print "Remote: $remoteUrl\n" if $remoteUrl;
    print "Branch: $branch\n" if $remoteUrl;
    print "--------------------------------------------------------\n\n";
}

sub main {
    my $startTime = time();

    processCLI();

    if ($remoteUrl) {
        processRemote();
    }

    # Get last imported revision
    my $revision = getRevision();

    if (!$remoteUrl) {
        # Verify if the $sourceDir folder is clean
        my $dirty = qx(git -C $sourceDir status --porcelain);
        chomp $dirty;
        if ($dirty) {
            die "Test262 at $sourceDir has unstaged/uncommited changes.";
        }
    }

    my ($newRevision, $newTracking) = getNewRevision();
    if (defined $revision and $newRevision eq $revision) {
        print 'Same revision, no need to import.';
        exit 0;
    }

    my $summary = getSummary();

    print "Summary of changes:\n$summary\n\n" if $summary;

    transferFiles();

    reportNewRevision($newRevision, $newTracking);
    saveSummary($summary);

    if ($remoteUrl) {
        rmtree($sourceDir);
    }

    my $endTime = time();
    my $totalTime = $endTime - $startTime;
    print "\nDone in $totalTime seconds!\n";
}

sub printAndRun {
    my ($command) = @_;

    print "> $command\n\n";
    return qx/$command/;
}

sub processRemote {
    $sourceDir = tempdir( CLEANUP => 1 );
    print "Importing Test262 from git\n";

    printAndRun("git clone -b $branch --depth=1 $remoteUrl $sourceDir");
}

sub transferFiles {
    # Remove previous Test262 folders
    printAndRun("rm -rf $test262Dir\/harness") if -e "$test262Dir/harness";
    printAndRun("rm -rf $test262Dir\/test") if -e "$test262Dir/test";

    # Copy from source
    printAndRun("mv $sourceDir\/harness $test262Dir");
    printAndRun("mv $sourceDir\/test $test262Dir");
}

sub getRevision {
    open(my $revfh, '<', $revisionFile) or die $!;

    my $revision;
    my $contents = join("\n", <$revfh>);

    # Some cheap yaml parsing, the YAML module is a possible alternative
    if ($contents =~ /test262 revision\: (\w*)/) {
        $revision = $1;
    }

    close($revfh);

    return $revision;
}

sub getNewRevision {
    my $tracking = qx/git -C $sourceDir ls-remote --get-url/;
    chomp $tracking;
    my $branch = qx/git -C $sourceDir rev-parse --abbrev-ref HEAD/;
    chomp $branch;
    my $revision = qx/git -C $sourceDir rev-parse HEAD/;
    chomp $revision;

    print "New tracking: $tracking\n";
    print "From branch: $branch\n";
    print "New revision: $revision\n\n";

    if (!$revision or !$tracking or !$branch) {
        die 'Something is wrong in the source git.';
    }

    return $revision, $tracking;
}

sub getSummary {
    my $summary = '';

    $summary = qx(git diff --no-index --name-status --diff-filter=ADRM -- $test262Dir/harness $sourceDir/harness);
    $summary .= qx(git diff --no-index --name-status --diff-filter=ADRM -- $test262Dir/test $sourceDir/test);
    chomp $summary;

    $summary =~ s/\s+$test262Dir\// /g;
    $summary =~ s/\s+$sourceDir\// /g;

    return $summary;
}

sub reportNewRevision {
    my ($revision, $tracking) = @_;

    open(my $fh, '>', $revisionFile) or die $!;

    print $fh "test262 remote url: $tracking\n";
    print $fh "test262 revision: $revision\n";

    close $fh;
}

sub saveSummary {
    my ($summary) = @_;

    open(my $fh, '>', $summaryFile) or die $!;

    print $fh $summary;

    close $fh;
}

__END__

=head1 DESCRIPTION

This program will import Test262 tests from a repository folder.

=head1 SYNOPSIS

Run using native Perl:

=over 8

test262-import
test262-import -s $source
test262-import -r https://github.com/tc39/test262
test262-import -r https://github.com/tc39/test262 -b es6

=back

=head1 OPTIONS

=over 8

=item B<--help, -h>

Print a brief help message.

=item B<--src, -s>

Specify the folder for Test262's repository.

=item B<--remote, -r>

Specify a remote Test262's repository. Defaults to 'git@github.com:tc39/test262.git'.

=item B<--branch, -b>

Specify a branch to be used for a remote Test262. Defaults to `master`.

=back
=cut
