#!/usr/bin/perl -w
#
# Copyright (C) 2011 Google Inc.  All rights reserved.
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
# This script runs the unittests specified in @testFiles.

use strict;
use warnings;

use Data::Dumper;
use File::Basename;
use File::Spec;
use FindBin;
use Getopt::Long;
use Test::More;
use lib File::Spec->catdir($FindBin::Bin, "..");
use LoadAsModule qw(PrepareChangeLog prepare-ChangeLog);

my %testFiles = ("perl_unittests.pl" => "perl",
                 "python_unittests.py" => "python",
                 "java_unittests.java" => "java");

my $resetResults;
GetOptions('reset-results' => \$resetResults);

my @testSet;
foreach my $testFile (sort keys %testFiles) {
    my $basename = $testFile;
    $basename = $1 if $basename =~ /^(.*)\.[^\.]*$/;
    push @testSet, {language => $testFiles{$testFile},
                    inputFile => File::Spec->catdir($FindBin::Bin, "resources", $testFile),
                    expectedFile => File::Spec->catdir($FindBin::Bin, "resources", $basename . "-expected.txt")};
}

plan(tests => scalar @testSet);
foreach my $test (@testSet) {
    open FH, "< $test->{inputFile}" or die "Cannot open $test->{inputFile}: $!";
    my $parser = eval "\\&PrepareChangeLog::get_function_line_ranges_for_$test->{language}";
    my @actualOutput = $parser->(\*FH, $test->{inputFile});;
    close FH;

    if ($resetResults) {
        open FH, "> $test->{expectedFile}" or die "Cannot open $test->{expectedFile}: $!";
        print FH Data::Dumper->new([\@actualOutput])->Terse(1)->Indent(1)->Dump();
        close FH;
        next;
    }

    open FH, "< $test->{expectedFile}" or die "Cannot open $test->{expectedFile}: $!";
    local $/ = undef;
    my $expectedOutput = eval <FH>;
    close FH;

    is_deeply(\@actualOutput, $expectedOutput, "Tests $test->{inputFile}");
}
