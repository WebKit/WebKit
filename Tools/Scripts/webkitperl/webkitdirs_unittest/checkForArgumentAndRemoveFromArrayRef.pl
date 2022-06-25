#!/usr/bin/env perl
#
# Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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

# Unit tests of webkitdirs::checkForArgumentAndRemoveFromArrayRef
use strict;
use warnings;
use Test::More;
use webkitdirs;

my @testCases = (
{
    argToCheck => "anything",
    args => [],
    expectedArgs => [],
    expectedReturn => 0,
    description => "Empty array"
},
{
    argToCheck => "not-found",
    args => ["a","b","c"],
    expectedArgs => ["a","b","c"],
    expectedReturn => 0,
    description => "Not found"
},
{
    argToCheck => "b",
    args => ["a","b","c"],
    expectedArgs => ["a","c"],
    expectedReturn => 1,
    description => "One occurrence"
},
{
    argToCheck => "a",
    args => ["a","b","a","c","a","x","a"],
    expectedArgs => ["b","c","x"],
    expectedReturn => 1,
    description => "More than one occurrence"
}
);

plan(tests => 2 * @testCases);

foreach my $testCase (@testCases) {
    my $result = checkForArgumentAndRemoveFromArrayRef($testCase->{argToCheck}, $testCase->{args});
    ok($result == $testCase->{expectedReturn}, "checkForArgumentAndRemoveFromArrayRef: $testCase->{description} - result");
    is_deeply($testCase->{args}, $testCase->{expectedArgs}, "checkForArgumentAndRemoveFromArrayRef: $testCase->{description} - array state");
}

