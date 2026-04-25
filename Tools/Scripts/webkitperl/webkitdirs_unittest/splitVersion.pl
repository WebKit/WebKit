#!/usr/bin/env perl

# Copyright (C) 2020 Apple Inc. All rights reserved.
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

# Unit tests for webkitdirs::parseAvailableXcodeSDKs($).

use strict;
use warnings;

use Config;
use Test::More;
use webkitdirs;

my @testCases = (
{
    string => "1.1",
    version => {major => 1, minor => 1, subminor => 0},
},
{
    string => "1.2.3",
    version => {major => 1, minor => 2, subminor => 3},
},
{
    string => "4.5.6-0",
    version => {major => 4, minor => 5, subminor => 6},
},
{
    string => "10.15.2",
    version => {major => 10, minor => 15, subminor => 2},
},
);

plan(tests => 1 * @testCases);

foreach my $testCase (@testCases) {
    my $result = splitVersionString($testCase->{string});
    is_deeply($result, $testCase->{version}, "Unexpected  parsed version for: $testCase->{string}");
}
