# Copyright (C) 2007, 2008, 2009 Apple Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer. 
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution. 
# 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Module to share code to work with various version control systems.
package VCSUtils;

use strict;
use warnings;

use Cwd qw();  # "qw()" prevents warnings about redefining getcwd() with "use POSIX;"
use File::Basename;
use File::Spec;

BEGIN {
    use Exporter   ();
    our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
    $VERSION     = 1.00;
    @ISA         = qw(Exporter);
    @EXPORT      = qw(
        &canonicalizePath
        &chdirReturningRelativePath
        &determineSVNRoot
        &determineVCSRoot
        &fixChangeLogPatch
        &gitBranch
        &gitdiff2svndiff
        &isGit
        &isGitBranchBuild
        &isGitDirectory
        &isSVN
        &isSVNDirectory
        &isSVNVersion16OrNewer
        &makeFilePathRelative
        &normalizePath
        &pathRelativeToSVNRepositoryRootForPath
        &svnRevisionForDirectory
        &svnStatus
    );
    %EXPORT_TAGS = ( );
    @EXPORT_OK   = ();
}

our @EXPORT_OK;

my $gitBranch;
my $gitRoot;
my $isGit;
my $isGitBranchBuild;
my $isSVN;
my $svnVersion;

sub isGitDirectory($)
{
    my ($dir) = @_;
    return system("cd $dir && git rev-parse > " . File::Spec->devnull() . " 2>&1") == 0;
}

sub isGit()
{
    return $isGit if defined $isGit;

    $isGit = isGitDirectory(".");
    return $isGit;
}

sub gitBranch()
{
    unless (defined $gitBranch) {
        chomp($gitBranch = `git symbolic-ref -q HEAD`);
        $gitBranch = "" if main::exitStatus($?); # FIXME: exitStatus is defined in webkitdirs.pm
        $gitBranch =~ s#^refs/heads/##;
        $gitBranch = "" if $gitBranch eq "master";
    }

    return $gitBranch;
}

sub isGitBranchBuild()
{
    my $branch = gitBranch();
    chomp(my $override = `git config --bool branch.$branch.webKitBranchBuild`);
    return 1 if $override eq "true";
    return 0 if $override eq "false";

    unless (defined $isGitBranchBuild) {
        chomp(my $gitBranchBuild = `git config --bool core.webKitBranchBuild`);
        $isGitBranchBuild = $gitBranchBuild eq "true";
    }

    return $isGitBranchBuild;
}

sub isSVNDirectory($)
{
    my ($dir) = @_;

    return -d File::Spec->catdir($dir, ".svn");
}

sub isSVN()
{
    return $isSVN if defined $isSVN;

    $isSVN = isSVNDirectory(".");
    return $isSVN;
}

sub svnVersion()
{
    return $svnVersion if defined $svnVersion;

    if (!isSVN()) {
        $svnVersion = 0;
    } else {
        chomp($svnVersion = `svn --version --quiet`);
    }
    return $svnVersion;
}

sub isSVNVersion16OrNewer()
{
    my $version = svnVersion();
    return eval "v$version" ge v1.6;
}

sub chdirReturningRelativePath($)
{
    my ($directory) = @_;
    my $previousDirectory = Cwd::getcwd();
    chdir $directory;
    my $newDirectory = Cwd::getcwd();
    return "." if $newDirectory eq $previousDirectory;
    return File::Spec->abs2rel($previousDirectory, $newDirectory);
}

sub determineGitRoot()
{
    chomp(my $gitDir = `git rev-parse --git-dir`);
    return dirname($gitDir);
}

sub determineSVNRoot()
{
    my $last = '';
    my $path = '.';
    my $parent = '..';
    my $repositoryRoot;
    my $repositoryUUID;
    while (1) {
        my $thisRoot;
        my $thisUUID;
        # Ignore error messages in case we've run past the root of the checkout.
        open INFO, "svn info '$path' 2> " . File::Spec->devnull() . " |" or die;
        while (<INFO>) {
            if (/^Repository Root: (.+)/) {
                $thisRoot = $1;
            }
            if (/^Repository UUID: (.+)/) {
                $thisUUID = $1;
            }
            if ($thisRoot && $thisUUID) {
                local $/ = undef;
                <INFO>; # Consume the rest of the input.
            }
        }
        close INFO;

        # It's possible (e.g. for developers of some ports) to have a WebKit
        # checkout in a subdirectory of another checkout.  So abort if the
        # repository root or the repository UUID suddenly changes.
        last if !$thisUUID;
        $repositoryUUID = $thisUUID if !$repositoryUUID;
        last if $thisUUID ne $repositoryUUID;

        last if !$thisRoot;
        $repositoryRoot = $thisRoot if !$repositoryRoot;
        last if $thisRoot ne $repositoryRoot;

        $last = $path;
        $path = File::Spec->catdir($parent, $path);
    }

    return File::Spec->rel2abs($last);
}

sub determineVCSRoot()
{
    if (isGit()) {
        return determineGitRoot();
    }

    if (!isSVN()) {
        # Some users have a workflow where svn-create-patch, svn-apply and
        # svn-unapply are used outside of multiple svn working directores,
        # so warn the user and assume Subversion is being used in this case.
        warn "Unable to determine VCS root; assuming Subversion";
        $isSVN = 1;
    }

    return determineSVNRoot();
}

sub svnRevisionForDirectory($)
{
    my ($dir) = @_;
    my $revision;

    if (isSVNDirectory($dir)) {
        my $svnInfo = `LC_ALL=C svn info $dir | grep Revision:`;
        ($revision) = ($svnInfo =~ m/Revision: (\d+).*/g);
    } elsif (isGitDirectory($dir)) {
        my $gitLog = `cd $dir && LC_ALL=C git log --grep='git-svn-id: ' -n 1 | grep git-svn-id:`;
        ($revision) = ($gitLog =~ m/ +git-svn-id: .+@(\d+) /g);
    }
    die "Unable to determine current SVN revision in $dir" unless (defined $revision);
    return $revision;
}

sub pathRelativeToSVNRepositoryRootForPath($)
{
    my ($file) = @_;
    my $relativePath = File::Spec->abs2rel($file);

    my $svnInfo;
    if (isSVN()) {
        $svnInfo = `LC_ALL=C svn info $relativePath`;
    } elsif (isGit()) {
        $svnInfo = `LC_ALL=C git svn info $relativePath`;
    }

    $svnInfo =~ /.*^URL: (.*?)$/m;
    my $svnURL = $1;

    $svnInfo =~ /.*^Repository Root: (.*?)$/m;
    my $repositoryRoot = $1;

    $svnURL =~ s/$repositoryRoot\///;
    return $svnURL;
}

sub makeFilePathRelative($)
{
    my ($path) = @_;
    return $path unless isGit();

    unless (defined $gitRoot) {
        chomp($gitRoot = `git rev-parse --show-cdup`);
    }
    return $gitRoot . $path;
}

sub normalizePath($)
{
    my ($path) = @_;
    $path =~ s/\\/\//g;
    return $path;
}

sub canonicalizePath($)
{
    my ($file) = @_;

    # Remove extra slashes and '.' directories in path
    $file = File::Spec->canonpath($file);

    # Remove '..' directories in path
    my @dirs = ();
    foreach my $dir (File::Spec->splitdir($file)) {
        if ($dir eq '..' && $#dirs >= 0 && $dirs[$#dirs] ne '..') {
            pop(@dirs);
        } else {
            push(@dirs, $dir);
        }
    }
    return ($#dirs >= 0) ? File::Spec->catdir(@dirs) : ".";
}

sub removeEOL($)
{
    my ($line) = @_;

    $line =~ s/[\r\n]+$//g;
    return $line;
}

sub svnStatus($)
{
    my ($fullPath) = @_;
    my $svnStatus;
    open SVN, "svn status --non-interactive --non-recursive '$fullPath' |" or die;
    if (-d $fullPath) {
        # When running "svn stat" on a directory, we can't assume that only one
        # status will be returned (since any files with a status below the
        # directory will be returned), and we can't assume that the directory will
        # be first (since any files with unknown status will be listed first).
        my $normalizedFullPath = File::Spec->catdir(File::Spec->splitdir($fullPath));
        while (<SVN>) {
            # Input may use a different EOL sequence than $/, so avoid chomp.
            $_ = removeEOL($_);
            my $normalizedStatPath = File::Spec->catdir(File::Spec->splitdir(substr($_, 7)));
            if ($normalizedFullPath eq $normalizedStatPath) {
                $svnStatus = "$_\n";
                last;
            }
        }
        # Read the rest of the svn command output to avoid a broken pipe warning.
        local $/ = undef;
        <SVN>;
    }
    else {
        # Files will have only one status returned.
        $svnStatus = removeEOL(<SVN>) . "\n";
    }
    close SVN;
    return $svnStatus;
}

sub gitdiff2svndiff($)
{
    $_ = shift @_;
    if (m#^diff --git a/(.+) b/(.+)#) {
        return "Index: $1";
    } elsif (m/^new file.*/) {
        return "";
    } elsif (m#^index [0-9a-f]{7}\.\.[0-9a-f]{7} [0-9]{6}#) {
        return "===================================================================";
    } elsif (m#^--- a/(.+)#) {
        return "--- $1";
    } elsif (m#^\+\+\+ b/(.+)#) {
        return "+++ $1";
    }
    return $_;
}

# The diff(1) command is greedy when matching lines, so a new ChangeLog entry will
# have lines of context at the top of a patch when the existing entry has the same
# date and author as the new entry.  Alter the ChangeLog patch so
# that the added lines ("+") in the patch always start at the beginning of the
# patch and there are no initial lines of context.
sub fixChangeLogPatch($)
{
    my $patch = shift; # $patch will only contain patch fragments for ChangeLog.

    $patch =~ /(\r?\n)/;
    my $lineEnding = $1;
    my @patchLines = split(/$lineEnding/, $patch);

    # e.g. 2009-06-03  Eric Seidel  <eric@webkit.org>
    my $dateLineRegexpString = '^\+(\d{4}-\d{2}-\d{2})' # Consume the leading '+' and the date.
                             . '\s+(.+)\s+' # Consume the name.
                             . '<([^<>]+)>$'; # And finally the email address.

    # Figure out where the patch contents start and stop.
    my $patchHeaderIndex;
    my $firstContentIndex;
    my $trailingContextIndex;
    my $dateIndex;
    my $patchEndIndex = scalar(@patchLines);
    for (my $index = 0; $index < @patchLines; ++$index) {
        my $line = $patchLines[$index];
        if ($line =~ /^\@\@ -\d+,\d+ \+\d+,\d+ \@\@$/) { # e.g. @@ -1,5 +1,18 @@
            if ($patchHeaderIndex) {
                $patchEndIndex = $index; # We only bother to fix up the first patch fragment.
                last;
            }
            $patchHeaderIndex = $index;
        }
        $firstContentIndex = $index if ($patchHeaderIndex && !$firstContentIndex && $line =~ /^\+[^+]/); # Only match after finding patchHeaderIndex, otherwise we'd match "+++".
        $dateIndex = $index if ($line =~ /$dateLineRegexpString/);
        $trailingContextIndex = $index if ($firstContentIndex && !$trailingContextIndex && $line =~ /^ /);
    }
    my $contentLineCount = $trailingContextIndex - $firstContentIndex;
    my $trailingContextLineCount = $patchEndIndex - $trailingContextIndex;

    # If we didn't find a date line in the content then this is not a patch we should try and fix.
    return $patch if (!$dateIndex);

    # We only need to do anything if the date line is not the first content line.
    return $patch if ($dateIndex == $firstContentIndex);

    # Write the new patch.
    my $totalNewContentLines = $contentLineCount + $trailingContextLineCount;
    $patchLines[$patchHeaderIndex] = "@@ -1,$trailingContextLineCount +1,$totalNewContentLines @@"; # Write a new header.
    my @repeatedLines = splice(@patchLines, $dateIndex, $trailingContextIndex - $dateIndex); # The date line and all the content after it that diff saw as repeated.
    splice(@patchLines, $firstContentIndex, 0, @repeatedLines); # Move the repeated content to the top.
    foreach my $line (@repeatedLines) {
        $line =~ s/^\+/ /;
    }
    splice(@patchLines, $trailingContextIndex, $patchEndIndex, @repeatedLines); # Replace trailing context with the repeated content.
    splice(@patchLines, $patchHeaderIndex + 1, $firstContentIndex - $patchHeaderIndex - 1); # Remove any leading context.

    return join($lineEnding, @patchLines) . "\n"; # patch(1) expects an extra trailing newline.
}

1;
