#!/usr/bin/env perl
#
# Copyright (C) 2011 Google Inc.  All rights reserved.
# Copyright (C) 2015-2016 Apple Inc.  All rights reserved.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public License
# along with this library; see the file COPYING.LIB.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301, USA.

# This script tests the parser of prepare-ChangeLog (i.e. get_function_line_ranges_for_XXXX()).
# This script runs the unittests in the 'resources' subdirectory.

use strict;
use warnings;

use Data::Dumper;
use File::Basename;
use File::Spec;
use File::Temp qw(tempfile);
use FindBin;
use Getopt::Long;
use Test::More;
use lib File::Spec->catdir($FindBin::Bin, "..");
use LoadAsModule qw(PrepareChangeLog prepare-ChangeLog);

sub captureOutput($);
sub convertAbsolutePathToRelativeUnixPath($$);
sub readTestFiles($);

use constant EXPECTED_RESULTS_SUFFIX => "-expected.txt";

my %methodForFileExtension = (
    ".cpp" => "get_function_line_ranges_for_cpp",
    ".css" => "get_selector_line_ranges_for_css",
    ".java" => "get_function_line_ranges_for_java",
    ".js" => "get_function_line_ranges_for_javascript",
    ".pl" => "get_function_line_ranges_for_perl",
    ".py" => "get_function_line_ranges_for_python",
    ".swift" => "get_function_line_ranges_for_swift",
);

my $resetResults;
GetOptions('reset-results' => \$resetResults);

my $resourcesDirectory = File::Spec->catdir($FindBin::Bin, "resources");
my @suffixList = keys %methodForFileExtension;
my @testFiles = readTestFiles($resourcesDirectory);
my @testSet;
foreach my $testFile (sort @testFiles) {
    my ($basename, undef, $extension) = fileparse($testFile, @suffixList);
    push @testSet, {
        method => $methodForFileExtension{$extension},
        inputFile => File::Spec->catfile($resourcesDirectory, $testFile),
        expectedFile => File::Spec->catfile($resourcesDirectory, $basename . EXPECTED_RESULTS_SUFFIX),
    };
}

plan(tests => scalar @testSet);
foreach my $test (@testSet) {
    open FH, "< $test->{inputFile}" or die "Cannot open $test->{inputFile}: $!";
    my $parser = eval "\\&PrepareChangeLog::$test->{method}";
    my @ranges;
    my ($stdout, $stderr) = captureOutput(sub { @ranges = $parser->(\*FH, $test->{inputFile}); });
    close FH;
    $stdout = convertAbsolutePathToRelativeUnixPath($stdout, $test->{inputFile});
    $stderr = convertAbsolutePathToRelativeUnixPath($stderr, $test->{inputFile});

    my %actualOutput = (ranges => \@ranges, stdout => $stdout, stderr => $stderr);
    if ($resetResults) {
        open FH, "> $test->{expectedFile}" or die "Cannot open $test->{expectedFile}: $!";
        print FH Data::Dumper->new([\%actualOutput])->Terse(1)->Indent(1)->Dump();
        close FH;
        next;
    }

    open FH, "< $test->{expectedFile}" or die "Cannot open $test->{expectedFile}: $!";
    local $/ = undef;
    my $expectedOutput = eval <FH>;
    close FH;

    is_deeply(\%actualOutput, $expectedOutput, "Tests $test->{inputFile}");
}

sub captureOutput($)
{
    my ($targetMethod) = @_;

    my ($stdoutFH, $stdoutFileName) = tempfile();
    my ($stderrFH, $stderrFileName) = tempfile();

    open OLDSTDOUT, ">&", \*STDOUT or die "Cannot dup STDOUT: $!";
    open OLDSTDERR, ">&", \*STDERR or die "Cannot dup STDERR: $!";

    open STDOUT, ">&", $stdoutFH or die "Cannot redirect STDOUT: $!";
    open STDERR, ">&", $stderrFH or die "Cannot redirect STDERR: $!";

    &$targetMethod();

    close STDOUT;
    close STDERR;

    open STDOUT, ">&OLDSTDOUT" or die "Cannot dup OLDSTDOUT: $!";
    open STDERR, ">&OLDSTDERR" or die "Cannot dup OLDSTDERR: $!";

    close OLDSTDOUT;
    close OLDSTDERR;

    seek $stdoutFH, 0, 0;
    seek $stderrFH, 0, 0;
    local $/ = undef;
    my $stdout = <$stdoutFH>;
    my $stderr = <$stderrFH>;

    close $stdoutFH;
    close $stderrFH;

    unlink $stdoutFileName or die "Cannot unlink $stdoutFileName: $!";
    unlink $stderrFileName or die "Cannot unlink $stderrFileName: $!";
    return ($stdout, $stderr);
}

sub convertAbsolutePathToRelativeUnixPath($$)
{
    my ($string, $path) = @_;
    my $sourceDir = LoadAsModule::unixPath(LoadAsModule::sourceDir());
    my $relativeUnixPath = LoadAsModule::unixPath($path);
    $sourceDir .= "/" unless $sourceDir =~ m-/$-;
    my $quotedSourceDir = quotemeta($sourceDir);
    $relativeUnixPath  =~ s/$quotedSourceDir//;
    my $quotedPath = quotemeta($path);
    $string =~ s/$quotedPath/$relativeUnixPath/g;
    $string =~ s/\r//g;
    return $string;
}

sub readTestFiles($)
{
    my ($directory) = @_;
    my @files;
    opendir(DIR, $directory) || die "Could not open $directory: $!";
    while (readdir(DIR)) {
        next if /^\./;
        next if ! -f File::Spec->catfile($directory, $_);
        next if /\Q@{[EXPECTED_RESULTS_SUFFIX]}\E$/;
        push @files, $_;
    }
    closedir(DIR);
    return @files;
}
