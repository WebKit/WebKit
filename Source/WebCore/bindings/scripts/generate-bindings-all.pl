#!/usr/bin/env perl
#
# Copyright (C) 2016 Sony Interactive Entertainment Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

use strict;
use warnings;
use FindBin;
use lib $FindBin::Bin;

use File::Basename;
use File::Spec;
use Getopt::Long;
use Cwd;
use English;
BEGIN { eval { require JSON::XS; JSON::XS->import(); 1 } or do { require JSON::PP; JSON::PP->import() } }

use IDLParser;
use CodeGenerator;

my $perl = $^X;
my $scriptDir = $FindBin::Bin;
my $outputDirectory;
my $idlFilesList;
my $ppIDLFilesList;
my $idlFileNamesList;
my $generator;
my @generatorDependency;
my $defines;
my $supplementalDependencyFile;
my @ppExtraOutput;
my @ppExtraArgs;
my $numOfJobs;
my $idlAttributesFile;
my $showProgress;
my @exclude;

GetOptions('outputDir=s' => \$outputDirectory,
           'idlFilesList=s' => \$idlFilesList,
           'ppIDLFilesList=s' => \$ppIDLFilesList,
           'idlFileNamesList=s' => \$idlFileNamesList,
           'generator=s' => \$generator,
           'generatorDependency=s@' => \@generatorDependency,
           'defines=s' => \$defines,
           'supplementalDependencyFile=s' => \$supplementalDependencyFile,
           'ppExtraOutput=s@' => \@ppExtraOutput,
           'ppExtraArgs=s@' => \@ppExtraArgs,
           'idlAttributesFile=s' => \$idlAttributesFile,
           'numOfJobs=i' => \$numOfJobs,
           'exclude=s@' => \@exclude,
           'showProgress' => \$showProgress);

if (!defined $numOfJobs) {
    $numOfJobs = `sysctl -n hw.activecpu 2>/dev/null` || `nproc 2>/dev/null` || 4;
    chomp $numOfJobs;
}

$idlFileNamesList = $idlFilesList if !defined $idlFileNamesList;

$| = 1;
my @idlFiles;
open(my $fh, '<', $idlFilesList) or die "Cannot open $idlFilesList";
@idlFiles = map { CygwinPathIfNeeded(s/\r?\n?$//r) } <$fh>;
close($fh) or die;

if (@exclude) {
    my %excluded = map { $_ => 1 } @exclude;
    @idlFiles = grep { !$excluded{basename($_)} } @idlFiles;
}

my @ppIDLFiles;
if ($ppIDLFilesList) {
    open($fh, '<', $ppIDLFilesList) or die "Cannot open $ppIDLFilesList";
    @ppIDLFiles = map { CygwinPathIfNeeded(s/\r?\n?$//r) } <$fh>;
    close($fh) or die;
}

my %oldSupplements;
my %newSupplements;
if ($supplementalDependencyFile) {
    if ($ppIDLFilesList) {
        my @output = ($supplementalDependencyFile, @ppExtraOutput);
        my @deps = ($ppIDLFilesList, @ppIDLFiles, @generatorDependency);
        if (needsUpdate(\@output, \@deps)) {
            readSupplementalDependencyFile($supplementalDependencyFile, \%oldSupplements) if -e $supplementalDependencyFile;
            my @args = (File::Spec->catfile($scriptDir, 'preprocess-idls.pl'),
                        '--defines', $defines,
                        '--idlFileNamesList', $ppIDLFilesList,
                        '--supplementalDependencyFile', $supplementalDependencyFile,
                        '--idlAttributesFile', $idlAttributesFile,
                        @ppExtraArgs);
            printProgress("Preprocess IDL");
            executeCommand($perl, @args) == 0 or die;
        }
    }
    readSupplementalDependencyFile($supplementalDependencyFile, \%newSupplements);
}

my %directoryCache;
buildDirectoryCache();

my @idlFilesToUpdate = grep &{sub {
    if (defined($oldSupplements{$_})
        && @{$oldSupplements{$_}} ne @{$newSupplements{$_} or []}) {
        # Re-process the IDL file if its supplemental dependencies were added or removed
        return 1;
    }
    my ($filename, $dirs, $suffix) = fileparse($_, '.idl');
    my $sourceFile = File::Spec->catfile($outputDirectory, "JS$filename.cpp");
    my $headerFile = File::Spec->catfile($outputDirectory, "JS$filename.h");
    my $depFile = File::Spec->catfile($outputDirectory, "JS$filename.dep");
    my @output = ($sourceFile, $headerFile);
    my @deps = ($_,
                $idlAttributesFile,
                @generatorDependency,
                @{$newSupplements{$_} or []},
                implicitDependencies($depFile));
    needsUpdate(\@output, \@deps);
}}, @idlFiles;

# Pre-parse shared data once in the parent process so forked children inherit it.
my %supplementalDependencies;
if ($supplementalDependencyFile) {
    open my $sdFh, '<', $supplementalDependencyFile or die "Cannot open $supplementalDependencyFile\n";
    while (my $line = <$sdFh>) {
        my ($idlFile, @followingIdlFiles) = split(/\s+/, $line);
        $supplementalDependencies{fileparse($idlFile)} = [sort @followingIdlFiles] if $idlFile;
    }
    close $sdFh;
}

my $idlAttributes;
{
    local $INPUT_RECORD_SEPARATOR;
    open(my $jsonFh, '<', $idlAttributesFile) or die "Couldn't open $idlAttributesFile: $!";
    my $input = <$jsonFh>;
    close($jsonFh);

    my $jsonDecoder = (eval { JSON::XS->new->utf8 } or JSON::PP->new->utf8);
    my $jsonHashRef = $jsonDecoder->decode($input);
    $idlAttributes = $jsonHashRef->{attributes};
}

# Pre-load the generator module so forked children don't need to compile it.
my $generatorModuleName = "CodeGenerator$generator.pm";
for my $dep (@generatorDependency) {
    if (basename($dep) eq $generatorModuleName) {
        my $dir = dirname($dep);
        unshift @INC, $dir;
        last;
    }
}
require $generatorModuleName;

# Pre-resolve realpath so forked children avoid per-file syscalls.
my @resolvedIdlFilesToUpdate;
for my $f (@idlFilesToUpdate) {
    push @resolvedIdlFilesToUpdate, Cwd::realpath($f);
}

my $abort = 0;
my $totalCount = @resolvedIdlFilesToUpdate;
my $currentCount = 0;

spawnGenerateBindingsIfNeeded() for (1 .. $numOfJobs);
while (waitpid(-1, 0) != -1) {
    if ($?) {
        $abort = 1;
    }
    spawnGenerateBindingsIfNeeded();
}
exit $abort;

sub needsUpdate
{
    my ($objects, $depends) = @_;
    my $oldestObjectTime;
    for (@$objects) {
        return 1 if !-f;
        my $m = mtime($_);
        if (!defined $oldestObjectTime || $m < $oldestObjectTime) {
            $oldestObjectTime = $m;
        }
    }
    for (@$depends) {
        die "Missing required dependency: $_" if !-f;
        my $m = mtime($_);
        if ($oldestObjectTime < $m) {
            return 1;
        }
    }
    return 0;
}

sub mtime
{
    my ($file) = @_;
    return (stat $file)[9];
}

sub spawnGenerateBindingsIfNeeded
{
    return if $abort;
    return unless @resolvedIdlFilesToUpdate;
    my $batchCount = int(($totalCount + $numOfJobs - 1) / $numOfJobs) || 1;
    my @files = splice(@resolvedIdlFilesToUpdate, 0, $batchCount);
    for (@files) {
        $currentCount++;
        my $basename = basename($_);
        printProgress("[$currentCount/$totalCount] $basename");
    }
    my $pid = fork();
    if ($pid == 0) {
        my $suppressVerboseOutput = 1;
        my $writeDependencies = 1;
        my $verbose = 0;
        for my $targetIdlFile (@files) {
            my $targetParser = IDLParser->new($suppressVerboseOutput);
            my $targetDocument = $targetParser->Parse($targetIdlFile, $defines, $idlAttributes);
            my $codeGen = CodeGenerator->new($generator, $outputDirectory, $outputDirectory, $writeDependencies, $verbose, $targetIdlFile, $idlAttributes, \%supplementalDependencies, $idlFileNamesList);
            $codeGen->ProcessDocument($targetDocument, $defines);
        }
        exit 0;
    }
    $abort = 1 unless defined $pid;
}

sub buildDirectoryCache
{
    open my $fh, "<", $idlFileNamesList or die "cannot open $idlFileNamesList for reading";
    while (<$fh>) {
        chomp $_;
        my $name = fileparse($_);
        $directoryCache{$name} = $_;
    }
    close $fh;
}

sub implicitDependencies
{
    my ($depFile) = @_;
    return () unless -f $depFile;
    open(my $fh, '<', $depFile) or die "Cannot open $depFile";
    my $firstLine = <$fh>;
    close($fh) or die;
    my (undef, $deps) = split(/ : /, $firstLine);
    my @deps = split(/\s+/, $deps);
    return map { $directoryCache{$_} or () } @deps;
}

sub executeCommand
{
    if ($^O eq 'MSWin32') {
        return system(quoteCommand(@_));
    }
    return system(@_);
}

sub quoteCommand
{
    return map {
        '"' . s/([\\\"])/\\$1/gr . '"';
    } @_;
}

sub CygwinPathIfNeeded
{
    my $path = shift;
    return Cygwin::win_to_posix_path($path) if ($^O eq 'cygwin');
    return $path;
}

sub readSupplementalDependencyFile
{
    my $filename = shift;
    my $supplements = shift;
    open(my $fh, '<', $filename) or die "Cannot open $filename";
    while (<$fh>) {
        my ($idlFile, @followingIdlFiles) = split(/\s+/);
        $supplements->{$idlFile} = [sort @followingIdlFiles];
    }
    close($fh) or die;
}

sub printProgress
{
    return unless $showProgress;
    my $msg = shift;
    print "$msg\n";
}
