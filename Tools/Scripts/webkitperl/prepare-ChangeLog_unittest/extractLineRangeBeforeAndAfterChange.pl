#!/usr/bin/env perl
#
# Copyright (C) 2015 Apple Inc. All rights reserved.
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

use strict;
use warnings;

use Test::More;
use FindBin;
use lib File::Spec->catdir($FindBin::Bin, "..");
use LoadAsModule qw(PrepareChangeLog prepare-ChangeLog);

my @testCaseHashRefs = (
####
#  Additions
##
{
    # New test
    testName => "add single line to the beginning of the file",
    inputText => "@@ -0,0 +1 @@",
    expectedResults => {
        beforeChange => [0, 0],
        afterChange => [1, 1],
    }
},
{
    # New test
    testName => "add two lines to the beginning of the file",
    inputText => "@@ -0,0 +1,2 @@",
    expectedResults => {
        beforeChange => [0, 0],
        afterChange => [1, 2],
    }
},
{
    # New test
    testName => "add two lines in the middle of the file",
    inputText => "@@ -4,0 +5,2 @@",
    expectedResults => {
        beforeChange => [4, 4],
        afterChange => [5, 6],
    }
},
{
    # New test
    testName => "append a single line to the end of the file",
    inputText => "@@ -1,0 +2 @@",
    expectedResults => {
        beforeChange => [1, 1],
        afterChange => [2, 2],
    }
},
####
#  Deletions
##
{
    # New test
    testName => "remove the first and only line in the file",
    inputText => "@@ -1 +0,0 @@",
    expectedResults => {
        beforeChange => [1, 1],
        afterChange => [0, 0],
    }
},
{
    # New test
    testName => "remove seven out of seven lines in the file",
    inputText => "@@ -1,7 +0,0 @@",
    expectedResults => {
        beforeChange => [1, 7],
        afterChange => [0, 0],
    }
},
{
    # New test
    testName => "remove two lines from the middle of the file",
    inputText => "@@ -4,2 +3,0 @@",
    expectedResults => {
        beforeChange => [4, 5],
        afterChange => [3, 3],
    }
},
####
#  Changes
##
{
    # New test
    testName => "change the first line of the file",
    inputText => "@@ -1 +1 @@",
    expectedResults => {
        beforeChange => [1, 1],
        afterChange => [1, 1],
    }
},
{
    # New test
    testName => "change the first line of the file and then append two more lines",
    inputText => "@@ -1 +1,3 @@",
    expectedResults => {
        beforeChange => [1, 1],
        afterChange => [1, 3],
    }
},
{
    # New test
    testName => "remove seven out of seven lines and then add one line",
    inputText => "@@ -1,7 +1 @@",
    expectedResults => {
        beforeChange => [1, 7],
        afterChange => [1, 1],
    }
},
{
    # New test
    testName => "remove two lines from the middle of the file",
    inputText => "@@ -4,2 +4,2 @@",
    expectedResults => {
        beforeChange => [4, 5],
        afterChange => [4, 5],
    }
},
{
    # New test
    testName => "remove two lines from the middle of the file and then add a line",
    inputText => "@@ -4,2 +4,3 @@",
    expectedResults => {
        beforeChange => [4, 5],
        afterChange => [4, 6],
    }
},
{
    # New test
    testName => "remove two lines from the middle of the file and then delete a line",
    inputText => "@@ -4,3 +4,2 @@",
    expectedResults => {
        beforeChange => [4, 6],
        afterChange => [4, 5],
    }
},
###
#  Invalid and malformed chunk ranges
##
# FIXME: We should make this set of tests more comprehensive.
{
    # New test
    testName => "[invalid] empty string",
    inputText => "",
    expectedResults => {
        beforeChange => [-1, -1],
        afterChange => [-1, -1],
    }
},
{
    # New test
    testName => "[invalid] bogus chunk range",
    inputText => "this is not a valid chunk range",
    expectedResults => {
        beforeChange => [-1, -1],
        afterChange => [-1, -1],
    }
},
{
    # New test
    testName => "[invalid] chunk range missing leading sentinel",
    inputText => "-4,3 +4,2 @@",
    expectedResults => {
        beforeChange => [-1, -1],
        afterChange => [-1, -1],
    }
},
);

my $testCasesCount = @testCaseHashRefs;
plan(tests => 2 * $testCasesCount); # Total number of assertions.

foreach my $testCase (@testCaseHashRefs) {
    my $expectedResults = $testCase->{expectedResults};

    my $testNameStart = "extractLineRangeBeforeChange(): $testCase->{testName}: comparing";
    my @got = PrepareChangeLog::extractLineRangeBeforeChange($testCase->{inputText});
    is_deeply(\@got, $expectedResults->{beforeChange}, "$testNameStart return value.");

    $testNameStart = "extractLineRangeAfterChange(): $testCase->{testName}: comparing";
    @got = PrepareChangeLog::extractLineRangeAfterChange($testCase->{inputText});
    is_deeply(\@got, $expectedResults->{afterChange}, "$testNameStart return value.");
}
