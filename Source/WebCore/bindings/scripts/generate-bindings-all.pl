#!/usr/bin/perl -w
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
use FindBin;
use lib $FindBin::Bin;

use File::Basename;
use File::Spec;
use File::Find;
use Getopt::Long;
use threads;
use threads::shared;
use Thread::Queue;

my $perl = $^X;
my $scriptDir = $FindBin::Bin;
my @idlDirectories;
my $outputDirectory;
my $idlFilesList;
my $generator;
my @generatorDependency;
my $defines;
my $preprocessor;
my $supplementalDependencyFile;
my @ppExtraOutput;
my @ppExtraArgs;
my $numOfJobs = 1;
my $idlAttributesFile;

GetOptions('include=s@' => \@idlDirectories,
           'outputDir=s' => \$outputDirectory,
           'idlFilesList=s' => \$idlFilesList,
           'generator=s' => \$generator,
           'generatorDependency=s@' => \@generatorDependency,
           'defines=s' => \$defines,
           'preprocessor=s' => \$preprocessor,
           'supplementalDependencyFile=s' => \$supplementalDependencyFile,
           'ppExtraOutput=s@' => \@ppExtraOutput,
           'ppExtraArgs=s@' => \@ppExtraArgs,
           'idlAttributesFile=s' => \$idlAttributesFile,
           'numOfJobs=i' => \$numOfJobs);

my @idlFiles;
open(my $fh, '<', $idlFilesList) or die "Cannot open $idlFilesList";
@idlFiles = <$fh>;
close($fh) or die;
s/\r?\n?$// for @idlFiles;

my %supplementedIdlFiles;
if ($supplementalDependencyFile) {
    my @output = ($supplementalDependencyFile, @ppExtraOutput);
    my @deps = (@idlFiles, @generatorDependency);
    if (needsUpdate(\@output, \@deps)) {
        my @args = ('--defines', $defines,
                    '--idlFilesList', $idlFilesList,
                    '--supplementalDependencyFile', $supplementalDependencyFile,
                    @ppExtraArgs);
        print("Preprocess IDL\n");
        system($perl, "$scriptDir/preprocess-idls.pl", @args) == 0 or die;
    }
    open(my $fh, '<', $supplementalDependencyFile) or die "Cannot open $supplementalDependencyFile";
    while (<$fh>) {
        my ($idlFile, @followingIdlFiles) = split(/\s+/);
        $supplementedIdlFiles{$idlFile} = \@followingIdlFiles;
    }
    close($fh) or die;
}

my @args = ('--defines', $defines,
            '--generator', $generator,
            '--outputDir', $outputDirectory,
            '--preprocessor', quoteCommand($preprocessor),
            '--idlAttributesFile', $idlAttributesFile,
            '--write-dependencies');
push @args, map { ('--include', $_) } @idlDirectories;
push @args, '--supplementalDependencyFile', $supplementalDependencyFile if $supplementalDependencyFile;

my %directoryCache;
buildDirectoryCache();

my $queue = Thread::Queue->new(@idlFiles);
my $abort :shared = 0;

my @threadPool = map { threads->create(\&worker) } (1 .. $numOfJobs);
$_->join for @threadPool;
exit $abort;

sub needsUpdate
{
    my ($objects, $depends) = @_;
    my $oldestObject;
    my $oldestObjectTime;
    for (@$objects) {
        return 1 if !-f;
        my $m = mtime($_);
        if (!defined $oldestObjectTime || $m < $oldestObjectTime) {
            $oldestObject = $_;
            $oldestObjectTime = $m;
        }
    }
    for (@$depends) {
        die "Missing required dependency: $_" if !-f;
        my $m = mtime($_);
        if ($oldestObjectTime < $m) {
            # print "$_ is newer than $oldestObject\n";
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

sub worker {
    while (my $file = $queue->dequeue_nb()) {
        last if $abort;
        eval {
            my ($filename, $dirs, $suffix) = fileparse($file, '.idl');
            my $sourceFile = File::Spec->catfile($outputDirectory, "JS$filename.cpp");
            my $headerFile = File::Spec->catfile($outputDirectory, "JS$filename.h");
            my $depFile = File::Spec->catfile($outputDirectory, "JS$filename.dep");
            my @output = ($sourceFile, $headerFile);
            my @deps = ($file,
                        $idlAttributesFile,
                        @generatorDependency,
                        @{$supplementedIdlFiles{$file} or []},
                        implicitDependencies($depFile));
            if (needsUpdate(\@output, \@deps)) {
                print "$filename$suffix\n";
                system($perl, "$scriptDir/generate-bindings.pl", @args, $file) == 0 or die;
            }
        };
        if ($@) {
            $abort = 1;
            die;
        }
    }
}

sub buildDirectoryCache
{
    my $wanted = sub {
        $directoryCache{$_} = $File::Find::name;
        $File::Find::prune = 1 unless ~/\./;
    };
    find($wanted, @idlDirectories);
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

sub quoteCommand
{
    use Config;
    if ($Config::Config{osname} eq 'MSWin32') {
        return map {
            s/\\/\\\\/g;
            s/\"/\\\"/g;
            "\"$_\"";
        } @_;
    }
    return @_;
}
