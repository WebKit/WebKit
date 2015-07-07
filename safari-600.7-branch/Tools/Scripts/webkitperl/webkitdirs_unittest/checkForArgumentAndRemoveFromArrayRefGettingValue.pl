#!/usr/bin/perl -w
#
# Copyright (C) 2014 Apple Inc. All rights reserved.
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

# Unit tests of webkitdirs::checkForArgumentAndRemoveFromArrayRefGettingValue
use strict;
use warnings;

use Test::More;
use webkitdirs;

my @testCases = (
{
    description => "empty string in empty array",
    argToCheck => "",
    args => [],
    expectedArgs => [],
    expectedReturn => undef,
},
{
    description => "non-existent option in empty array",
    argToCheck => "--sdk",
    args => [],
    expectedArgs => [],
    expectedReturn => undef,
},
{
    description => "non-existent option in non-empty array",
    argToCheck => "--sdk",
    args => ["--clean", "--debug"],
    expectedArgs => ["--clean", "--debug"],
    expectedReturn => undef,
},
{
    description => "option with specified value",
    argToCheck => "--sdk",
    args => ["--clean", "--debug", "--sdk", "iphonesimulator"],
    expectedArgs => ["--clean", "--debug"],
    expectedReturn => "iphonesimulator",
},
{
    description => "option with specified value whose value follows an equal sign",
    argToCheck => "--sdk",
    args => ["--clean", "--debug", "--sdk=iphonesimulator"],
    expectedArgs => ["--clean", "--debug"],
    expectedReturn => "iphonesimulator",
},
{
    description => "option whose value is the empty string and follows an equal sign", # Unrecognized argument; no change 
    argToCheck => "--sdk",
    args => ["--clean", "--debug", "--sdk=", "--no-webkit2"],
    expectedArgs => ["--clean", "--debug", "--sdk=", "--no-webkit2"],
    expectedReturn => undef,
},
{
    description => "multiple options of the same name",
    argToCheck => "--sdk",
    args => ["--clean", "--debug", "--sdk=iphonesimulator", "--sdk=macosx"],
    expectedArgs => ["--clean", "--debug", "--sdk=macosx"],
    expectedReturn => "iphonesimulator",
},
####
# Malformed input
##
# We shouldn't encounter such input in practice.
{
    description => "malformed: option with unspecified value at the end of the array",
    argToCheck => "--sdk",
    args => ["--clean", "--debug", "--sdk"],
    expectedArgs => ["--clean", "--debug"],
    expectedReturn => undef,
},
{
    description => "malformed: option with unspecified value",
    argToCheck => "--sdk",
    args => ["--clean", "--debug", "--sdk", "--no-webkit2"],
    expectedArgs => ["--clean", "--debug"],
    expectedReturn => "--no-webkit2",
},
{
    description => "malformed: multiple options of the same name; first occurrence has unspecified value",
    argToCheck => "--sdk",
    args => ["--clean", "--debug", "--sdk", "--sdk=iphonesimulator", "--sdk=macosx"],
    expectedArgs => ["--clean", "--debug", "--sdk=macosx"],
    expectedReturn => "--sdk=iphonesimulator",
},
);

plan(tests => 3 * @testCases);

foreach my $testCase (@testCases) {
    my $testNameStart = "checkForArgumentAndRemoveFromArrayRef(): $testCase->{description}: comparing";
    my $actualValue;
    my @shadowArgs = @{$testCase->{args}}; # Copy the arguments since checkForArgumentAndRemoveFromArrayRefGettingValue() mutates the specified array. 
    is(checkForArgumentAndRemoveFromArrayRefGettingValue($testCase->{argToCheck}, \$actualValue, \@shadowArgs), $testCase->{expectedReturn}, "$testNameStart return value");
    is($actualValue, $testCase->{expectedReturn}, "$testNameStart out parameter");
    is_deeply(\@shadowArgs, $testCase->{expectedArgs}, "$testNameStart array");
}
