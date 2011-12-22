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

my @testFiles = qw(perl_unittests.pl);

my @inputFiles = map { File::Spec->catdir($FindBin::Bin, "resources", $_) } @testFiles;
my @expectedFiles = map { s/^(.*)\.[^\.]*$/$1/; File::Spec->catdir($FindBin::Bin, "resources", $_ . "-expected.txt") } @testFiles;

my $resetResults;
GetOptions('reset-results' => \$resetResults);

my $testCount = @testFiles;
plan(tests => $testCount);
for (my $index = 0; $index < $testCount; $index++) {
    my $inputFile = $inputFiles[$index];
    my $expectedFile = $expectedFiles[$index];

    open FH, "< $inputFile" or die "Cannot open $inputFile: $!";
    my @actualOutput = PrepareChangeLog::get_function_line_ranges_for_perl(\*FH, $inputFile);
    close FH;

    if ($resetResults) {
        open FH, "> $expectedFile" or die "Cannot open $expectedFile: $!";
        print FH Data::Dumper->new([\@actualOutput])->Terse(1)->Indent(1)->Dump();
        close FH;
        next;
    }

    open FH, "< $expectedFile" or die "Cannot open $expectedFile: $!";
    local $/ = undef;
    my $expectedOutput = eval <FH>;
    close FH;

    is_deeply(\@actualOutput, $expectedOutput, "Tests $inputFile");
}
