# Copyright (C) 2007, 2008, 2009 Apple Inc.  All rights reserved.
# Copyright (C) 2009, 2010 Chris Jerdonek (chris.jerdonek@gmail.com)
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
use English; # for $POSTMATCH, etc.
use File::Basename;
use File::Spec;
use POSIX;

BEGIN {
    use Exporter   ();
    our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
    $VERSION     = 1.00;
    @ISA         = qw(Exporter);
    @EXPORT      = qw(
        &canonicalizePath
        &changeLogEmailAddress
        &changeLogName
        &chdirReturningRelativePath
        &decodeGitBinaryPatch
        &determineSVNRoot
        &determineVCSRoot
        &exitStatus
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
        &parsePatch
        &pathRelativeToSVNRepositoryRootForPath
        &runPatchCommand
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

# This method is for portability. Return the system-appropriate exit
# status of a child process.
#
# Args: pass the child error status returned by the last pipe close,
#       for example "$?".
sub exitStatus($)
{
    my ($returnvalue) = @_;
    if ($^O eq "MSWin32") {
        return $returnvalue >> 8;
    }
    return WEXITSTATUS($returnvalue);
}

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
        $gitBranch = "" if exitStatus($?);
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

# Convert a line of a git-formatted patch to SVN format, while
# preserving any end-of-line characters.
sub gitdiff2svndiff($)
{
    $_ = shift @_;

    if (m#^diff --git \w/(.+) \w/([^\r\n]+)#) {
        return "Index: $1$POSTMATCH";
    }
    if (m#^index [0-9a-f]{7}\.\.[0-9a-f]{7} [0-9]{6}#) {
        # FIXME: No need to return dividing line once parseDiffHeader() is used.
        return "===================================================================$POSTMATCH";
    }
    if (m#^--- \w/([^\r\n]+)#) {
        return "--- $1$POSTMATCH";
    }
    if (m#^\+\+\+ \w/([^\r\n]+)#) {
        return "+++ $1$POSTMATCH";
    }
    return $_;
}

# Parse the next diff header from the given file handle, and advance
# the file handle so the last line read is the first line after the
# parsed header block.
#
# This subroutine dies if given leading junk or if the end of the header
# block could not be detected. The last line of a header block is a
# line beginning with "+++".
#
# Args:
#   $fileHandle: advanced so the last line read is the first line of the
#                next diff header. For SVN-formatted diffs, this is the
#                "Index:" line.
#   $line: the line last read from $fileHandle
#
# Returns ($headerHashRef, $lastReadLine):
#   $headerHashRef: a hash reference representing a diff header
#     copiedFromPath: if a file copy, the path from which the file was
#                     copied. Otherwise, undefined.
#     indexPath: the path in the "Index:" line.
#     sourceRevision: the revision number of the source. This is the same
#                     as the revision number the file was copied from, in
#                     the case of a file copy.
#     svnConvertedText: the header text converted to SVN format.
#                       Unrecognized lines are discarded.
#   $lastReadLine: the line last read from $fileHandle. This is the first
#                  line after the header ending.
sub parseDiffHeader($$)
{
    my ($fileHandle, $line) = @_;

    my $filter;
    if ($line =~ m#^diff --git #) {
        $filter = \&gitdiff2svndiff;
    }
    $line = &$filter($line) if $filter;

    my $indexPath;
    if ($line =~ /^Index: ([^\r\n]+)/) {
        $indexPath = $1;
    } else {
        die("Could not parse first line of diff header: \"$line\".");
    }

    my %header;

    my $foundHeaderEnding;
    my $lastReadLine; 
    my $sourceRevision;
    my $svnConvertedText = $line;
    while (<$fileHandle>) {
        # Temporarily strip off any end-of-line characters to simplify
        # regex matching below.
        s/([\n\r]+)$//;
        my $eol = $1;

        $_ = &$filter($_) if $filter;

        # Fix paths on ""---" and "+++" lines to match the leading
        # index line.
        if (s/^--- \S+/--- $indexPath/) {
            # ---
            if (/^--- .+\(revision (\d+)\)/) {
                $sourceRevision = $1 if ($1 != 0);
                if (/\(from (\S+):(\d+)\)$/) {
                    # The "from" clause is created by svn-create-patch, in
                    # which case there is always also a "revision" clause.
                    $header{copiedFromPath} = $1;
                    die("Revision number \"$2\" in \"from\" clause does not match " .
                        "source revision number \"$sourceRevision\".") if ($2 != $sourceRevision);
                }
            }
            $_ = "=" x 67 . "$eol$_"; # Prepend dividing line ===....
        } elsif (s/^\+\+\+ \S+/+++ $indexPath/) {
            # +++
            $foundHeaderEnding = 1;
        } else {
            # Skip unrecognized lines.
            next;
        }

        $svnConvertedText .= "$_$eol"; # Also restore end-of-line characters.
        if ($foundHeaderEnding) {
            $lastReadLine = <$fileHandle>;
            last;
        }
    } # $lastReadLine is undef if while loop ran out.

    if (!$foundHeaderEnding) {
        die("Did not find end of header block corresponding to index path \"$indexPath\".");
    }

    $header{indexPath} = $indexPath;
    $header{sourceRevision} = $sourceRevision;
    $header{svnConvertedText} = $svnConvertedText;

    return (\%header, $lastReadLine);
}

# Parse one diff from a patch file created by svn-create-patch, and
# advance the file handle so the last line read is the first line
# of the next header block.
#
# This subroutine preserves any leading junk encountered before the header.
#
# Args:
#   $fileHandle: a file handle advanced to the first line of the next
#                header block. Leading junk is okay.
#   $line: the line last read from $fileHandle.
#
# Returns ($diffHashRef, $lastReadLine):
#   $diffHashRef:
#     copiedFromPath: if a file copy, the path from which the file was
#                     copied. Otherwise, undefined.
#     indexPath: the path in the "Index:" line.
#     sourceRevision: the revision number of the source. This is the same
#                     as the revision number the file was copied from, in
#                     the case of a file copy.
#     svnConvertedText: the diff converted to SVN format.
#   $lastReadLine: the line last read from $fileHandle
sub parseDiff($$)
{
    my ($fileHandle, $line) = @_;

    my $headerStartRegEx = qr#^Index: #; # SVN-style header for the default
    my $gitHeaderStartRegEx = qr#^diff --git \w/#;

    my $headerHashRef; # Last header found, as returned by parseDiffHeader().
    my $svnText;
    while (defined($line)) {
        if (!$headerHashRef && ($line =~ $gitHeaderStartRegEx)) {
            # Then assume all diffs in the patch are Git-formatted. This
            # block was made to be enterable at most once since we assume
            # all diffs in the patch are formatted the same (SVN or Git).
            $headerStartRegEx = $gitHeaderStartRegEx;
        }

        if ($line !~ $headerStartRegEx) {
            # Then we are in the body of the diff.
            $svnText .= $line;
            $line = <$fileHandle>;
            next;
        } # Otherwise, we found a diff header.

        if ($headerHashRef) {
            # Then this is the second diff header of this while loop.
            last;
        }

        ($headerHashRef, $line) = parseDiffHeader($fileHandle, $line);

        $svnText .= $headerHashRef->{svnConvertedText};
    }

    my %diffHashRef;
    $diffHashRef{copiedFromPath} = $headerHashRef->{copiedFromPath};
    $diffHashRef{indexPath} = $headerHashRef->{indexPath};
    $diffHashRef{sourceRevision} = $headerHashRef->{sourceRevision};
    $diffHashRef{svnConvertedText} = $svnText;

    return (\%diffHashRef, $line);
}

# Parse a patch file created by svn-create-patch.
#
# Args:
#   $fileHandle: A file handle to the patch file that has not yet been
#                read from.
#
# Returns:
#   @diffHashRefs: an array of diff hash references. See parseDiff() for
#                  a description of each $diffHashRef.
sub parsePatch($)
{
    my ($fileHandle) = @_;

    my @diffHashRefs; # return value

    my $line = <$fileHandle>;

    while (defined($line)) { # Otherwise, at EOF.

        my $diffHashRef;
        ($diffHashRef, $line) = parseDiff($fileHandle, $line);

        push @diffHashRefs, $diffHashRef;
    }

    return @diffHashRefs;
}

# If possible, returns a ChangeLog patch equivalent to the given one,
# but with the newest ChangeLog entry inserted at the top of the
# file -- i.e. no leading context and all lines starting with "+".
#
# If given a patch string not representable as a patch with the above
# properties, it returns the input back unchanged.
#
# WARNING: This subroutine can return an inequivalent patch string if
# both the beginning of the new ChangeLog file matches the beginning
# of the source ChangeLog, and the source beginning was modified.
# Otherwise, it is guaranteed to return an equivalent patch string,
# if it returns.
#
# Applying this subroutine to ChangeLog patches allows svn-apply to
# insert new ChangeLog entries at the top of the ChangeLog file.
# svn-apply uses patch with --fuzz=3 to do this. We need to apply
# this subroutine because the diff(1) command is greedy when matching
# lines. A new ChangeLog entry with the same date and author as the
# previous will match and cause the diff to have lines of starting
# context.
#
# This subroutine has unit tests in VCSUtils_unittest.pl.
sub fixChangeLogPatch($)
{
    my $patch = shift; # $patch will only contain patch fragments for ChangeLog.

    $patch =~ /(\r?\n)/;
    my $lineEnding = $1;
    my @lines = split(/$lineEnding/, $patch);

    my $i = 0; # We reuse the same index throughout.

    # Skip to beginning of first chunk.
    for (; $i < @lines; ++$i) {
        if (substr($lines[$i], 0, 1) eq "@") {
            last;
        }
    }
    my $chunkStartIndex = ++$i;

    # Optimization: do not process if new lines already begin the chunk.
    if (substr($lines[$i], 0, 1) eq "+") {
        return $patch;
    }

    # Skip to first line of newly added ChangeLog entry.
    # For example, +2009-06-03  Eric Seidel  <eric@webkit.org>
    my $dateStartRegEx = '^\+(\d{4}-\d{2}-\d{2})' # leading "+" and date
                         . '\s+(.+)\s+' # name
                         . '<([^<>]+)>$'; # e-mail address

    for (; $i < @lines; ++$i) {
        my $line = $lines[$i];
        my $firstChar = substr($line, 0, 1);
        if ($line =~ /$dateStartRegEx/) {
            last;
        } elsif ($firstChar eq " " or $firstChar eq "+") {
            next;
        }
        return $patch; # Do not change if, for example, "-" or "@" found.
    }
    if ($i >= @lines) {
        return $patch; # Do not change if date not found.
    }
    my $dateStartIndex = $i;

    # Rewrite overlapping lines to lead with " ".
    my @overlappingLines = (); # These will include a leading "+".
    for (; $i < @lines; ++$i) {
        my $line = $lines[$i];
        if (substr($line, 0, 1) ne "+") {
          last;
        }
        push(@overlappingLines, $line);
        $lines[$i] = " " . substr($line, 1);
    }

    # Remove excess ending context, if necessary.
    my $shouldTrimContext = 1;
    for (; $i < @lines; ++$i) {
        my $firstChar = substr($lines[$i], 0, 1);
        if ($firstChar eq " ") {
            next;
        } elsif ($firstChar eq "@") {
            last;
        }
        $shouldTrimContext = 0; # For example, if "+" or "-" encountered.
        last;
    }
    my $deletedLineCount = 0;
    if ($shouldTrimContext) { # Also occurs if end of file reached.
        splice(@lines, $i - @overlappingLines, @overlappingLines);
        $deletedLineCount = @overlappingLines;
    }

    # Work backwards, shifting overlapping lines towards front
    # while checking that patch stays equivalent.
    for ($i = $dateStartIndex - 1; $i >= $chunkStartIndex; --$i) {
        my $line = $lines[$i];
        if (substr($line, 0, 1) ne " ") {
            next;
        }
        my $text = substr($line, 1);
        my $newLine = pop(@overlappingLines);
        if ($text ne substr($newLine, 1)) {
            return $patch; # Unexpected difference.
        }
        $lines[$i] = "+$text";
    }

    # Finish moving whatever overlapping lines remain, and update
    # the initial chunk range.
    my $chunkRangeRegEx = '^\@\@ -(\d+),(\d+) \+\d+,(\d+) \@\@$'; # e.g. @@ -2,6 +2,18 @@
    if ($lines[$chunkStartIndex - 1] !~ /$chunkRangeRegEx/) {
        # FIXME: Handle errors differently from ChangeLog files that
        # are okay but should not be altered. That way we can find out
        # if improvements to the script ever become necessary.
        return $patch; # Error: unexpected patch string format.
    }
    my $skippedFirstLineCount = $1 - 1;
    my $oldSourceLineCount = $2;
    my $oldTargetLineCount = $3;

    if (@overlappingLines != $skippedFirstLineCount) {
        # This can happen, for example, when deliberately inserting
        # a new ChangeLog entry earlier in the file.
        return $patch;
    }
    # If @overlappingLines > 0, this is where we make use of the
    # assumption that the beginning of the source file was not modified.
    splice(@lines, $chunkStartIndex, 0, @overlappingLines);

    my $sourceLineCount = $oldSourceLineCount + @overlappingLines - $deletedLineCount;
    my $targetLineCount = $oldTargetLineCount + @overlappingLines - $deletedLineCount;
    $lines[$chunkStartIndex - 1] = "@@ -1,$sourceLineCount +1,$targetLineCount @@";

    return join($lineEnding, @lines) . "\n"; # patch(1) expects an extra trailing newline.
}

# This is a supporting method for runPatchCommand.
#
# Arg: the optional $args parameter passed to runPatchCommand (can be undefined).
#
# Returns ($patchCommand, $isForcing).
#
# This subroutine has unit tests in VCSUtils_unittest.pl.
sub generatePatchCommand($)
{
    my ($passedArgsHashRef) = @_;

    my $argsHashRef = { # Defaults
        ensureForce => 0,
        shouldReverse => 0,
        options => []
    };
    
    # Merges hash references. It's okay here if passed hash reference is undefined.
    @{$argsHashRef}{keys %{$passedArgsHashRef}} = values %{$passedArgsHashRef};
    
    my $ensureForce = $argsHashRef->{ensureForce};
    my $shouldReverse = $argsHashRef->{shouldReverse};
    my $options = $argsHashRef->{options};

    if (! $options) {
        $options = [];
    } else {
        $options = [@{$options}]; # Copy to avoid side effects.
    }

    my $isForcing = 0;
    if (grep /^--force$/, @{$options}) {
        $isForcing = 1;
    } elsif ($ensureForce) {
        push @{$options}, "--force";
        $isForcing = 1;
    }

    if ($shouldReverse) { # No check: --reverse should never be passed explicitly.
        push @{$options}, "--reverse";
    }

    @{$options} = sort(@{$options}); # For easier testing.

    my $patchCommand = join(" ", "patch -p0", @{$options});

    return ($patchCommand, $isForcing);
}

# Apply the given patch using the patch(1) command.
#
# On success, return the resulting exit status. Otherwise, exit with the
# exit status. If "--force" is passed as an option, however, then never
# exit and always return the exit status.
#
# Args:
#   $patch: a patch string.
#   $repositoryRootPath: an absolute path to the repository root.
#   $pathRelativeToRoot: the path of the file to be patched, relative to the
#                        repository root. This should normally be the path
#                        found in the patch's "Index:" line. It is passed
#                        explicitly rather than reparsed from the patch
#                        string for optimization purposes.
#                            This is used only for error reporting. The
#                        patch command gleans the actual file to patch
#                        from the patch string.
#   $args: a reference to a hash of optional arguments. The possible
#          keys are --
#            ensureForce: whether to ensure --force is passed (defaults to 0).
#            shouldReverse: whether to pass --reverse (defaults to 0).
#            options: a reference to an array of options to pass to the
#                     patch command. The subroutine passes the -p0 option
#                     no matter what. This should not include --reverse.
#
# This subroutine has unit tests in VCSUtils_unittest.pl.
sub runPatchCommand($$$;$)
{
    my ($patch, $repositoryRootPath, $pathRelativeToRoot, $args) = @_;

    my ($patchCommand, $isForcing) = generatePatchCommand($args);

    # Temporarily change the working directory since the path found
    # in the patch's "Index:" line is relative to the repository root
    # (i.e. the same as $pathRelativeToRoot).
    my $cwd = Cwd::getcwd();
    chdir $repositoryRootPath;

    open PATCH, "| $patchCommand" or die "Could not call \"$patchCommand\" for file \"$pathRelativeToRoot\": $!";
    print PATCH $patch;
    close PATCH;
    my $exitStatus = exitStatus($?);

    chdir $cwd;

    if ($exitStatus && !$isForcing) {
        print "Calling \"$patchCommand\" for file \"$pathRelativeToRoot\" returned " .
              "status $exitStatus.  Pass --force to ignore patch failures.\n";
        exit $exitStatus;
    }

    return $exitStatus;
}

sub gitConfig($)
{
    return unless $isGit;

    my ($config) = @_;

    my $result = `git config $config`;
    if (($? >> 8)) {
        $result = `git repo-config $config`;
    }
    chomp $result;
    return $result;
}

sub changeLogNameError($)
{
    my ($message) = @_;
    print STDERR "$message\nEither:\n";
    print STDERR "  set CHANGE_LOG_NAME in your environment\n";
    print STDERR "  OR pass --name= on the command line\n";
    print STDERR "  OR set REAL_NAME in your environment";
    print STDERR "  OR git users can set 'git config user.name'\n";
    exit(1);
}

sub changeLogName()
{
    my $name = $ENV{CHANGE_LOG_NAME} || $ENV{REAL_NAME} || gitConfig("user.name") || (split /\s*,\s*/, (getpwuid $<)[6])[0];

    changeLogNameError("Failed to determine ChangeLog name.") unless $name;
    # getpwuid seems to always succeed on windows, returning the username instead of the full name.  This check will catch that case.
    changeLogNameError("'$name' does not contain a space!  ChangeLogs should contain your full name.") unless ($name =~ /\w \w/);

    return $name;
}

sub changeLogEmailAddressError($)
{
    my ($message) = @_;
    print STDERR "$message\nEither:\n";
    print STDERR "  set CHANGE_LOG_EMAIL_ADDRESS in your environment\n";
    print STDERR "  OR pass --email= on the command line\n";
    print STDERR "  OR set EMAIL_ADDRESS in your environment\n";
    print STDERR "  OR git users can set 'git config user.email'\n";
    exit(1);
}

sub changeLogEmailAddress()
{
    my $emailAddress = $ENV{CHANGE_LOG_EMAIL_ADDRESS} || $ENV{EMAIL_ADDRESS} || gitConfig("user.email");

    changeLogEmailAddressError("Failed to determine email address for ChangeLog.") unless $emailAddress;
    changeLogEmailAddressError("Email address '$emailAddress' does not contain '\@' and is likely invalid.") unless ($emailAddress =~ /\@/);

    return $emailAddress;
}

# http://tools.ietf.org/html/rfc1924
sub decodeBase85($)
{
    my ($encoded) = @_;
    my %table;
    my @characters = ('0'..'9', 'A'..'Z', 'a'..'z', '!', '#', '$', '%', '&', '(', ')', '*', '+', '-', ';', '<', '=', '>', '?', '@', '^', '_', '`', '{', '|', '}', '~');
    for (my $i = 0; $i < 85; $i++) {
        $table{$characters[$i]} = $i;
    }

    my $decoded = '';
    my @encodedChars = $encoded =~ /./g;

    for (my $encodedIter = 0; defined($encodedChars[$encodedIter]);) {
        my $digit = 0;
        for (my $i = 0; $i < 5; $i++) {
            $digit *= 85;
            my $char = $encodedChars[$encodedIter];
            $digit += $table{$char};
            $encodedIter++;
        }

        for (my $i = 0; $i < 4; $i++) {
            $decoded .= chr(($digit >> (3 - $i) * 8) & 255);
        }
    }

    return $decoded;
}

sub decodeGitBinaryChunk($$)
{
    my ($contents, $fullPath) = @_;

    # Load this module lazily in case the user don't have this module
    # and won't handle git binary patches.
    require Compress::Zlib;

    my $encoded = "";
    my $compressedSize = 0;
    while ($contents =~ /^([A-Za-z])(.*)$/gm) {
        my $line = $2;
        next if $line eq "";
        die "$fullPath: unexpected size of a line: $&" if length($2) % 5 != 0;
        my $actualSize = length($2) / 5 * 4;
        my $encodedExpectedSize = ord($1);
        my $expectedSize = $encodedExpectedSize <= ord("Z") ? $encodedExpectedSize - ord("A") + 1 : $encodedExpectedSize - ord("a") + 27;

        die "$fullPath: unexpected size of a line: $&" if int(($expectedSize + 3) / 4) * 4 != $actualSize;
        $compressedSize += $expectedSize;
        $encoded .= $line;
    }

    my $compressed = decodeBase85($encoded);
    $compressed = substr($compressed, 0, $compressedSize);
    return Compress::Zlib::uncompress($compressed);
}

sub decodeGitBinaryPatch($$)
{
    my ($contents, $fullPath) = @_;

    # Git binary patch has two chunks. One is for the normal patching
    # and another is for the reverse patching.
    #
    # Each chunk a line which starts from either "literal" or "delta",
    # followed by a number which specifies decoded size of the chunk.
    # The "delta" type chunks aren't supported by this function yet.
    #
    # Then, content of the chunk comes. To decode the content, we
    # need decode it with base85 first, and then zlib.
    my $gitPatchRegExp = '(literal|delta) ([0-9]+)\n([A-Za-z0-9!#$%&()*+-;<=>?@^_`{|}~\\n]*?)\n\n';
    if ($contents !~ m"\nGIT binary patch\n$gitPatchRegExp$gitPatchRegExp\Z") {
        die "$fullPath: unknown git binary patch format"
    }

    my $binaryChunkType = $1;
    my $binaryChunkExpectedSize = $2;
    my $encodedChunk = $3;
    my $reverseBinaryChunkType = $4;
    my $reverseBinaryChunkExpectedSize = $5;
    my $encodedReverseChunk = $6;

    my $binaryChunk = decodeGitBinaryChunk($encodedChunk, $fullPath);
    my $binaryChunkActualSize = length($binaryChunk);
    my $reverseBinaryChunk = decodeGitBinaryChunk($encodedReverseChunk, $fullPath);
    my $reverseBinaryChunkActualSize = length($reverseBinaryChunk);

    die "$fullPath: unexpected size of the first chunk (expected $binaryChunkExpectedSize but was $binaryChunkActualSize" if ($binaryChunkExpectedSize != $binaryChunkActualSize);
    die "$fullPath: unexpected size of the second chunk (expected $reverseBinaryChunkExpectedSize but was $reverseBinaryChunkActualSize" if ($reverseBinaryChunkExpectedSize != $reverseBinaryChunkActualSize);

    return ($binaryChunkType, $binaryChunk, $reverseBinaryChunkType, $reverseBinaryChunk);
}

1;
