#!/usr/bin/env perl

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

# Unit tests for webkitdirs::appendToEnvironmentVariableList($$).

use strict;
use warnings;

use Config;
use Test::More;
use webkitdirs;

my @testCases = (
{
    setup => sub { delete $ENV{DYLD_FRAMEWORK_PATH}; },
    argName => "DYLD_FRAMEWORK_PATH",
    argValue => "/System/Library/PrivateFrameworks",
    expectedValue => "/System/Library/PrivateFrameworks",
    description => "Append to nonexistent variable",
},
{
    setup => sub { $ENV{DYLD_FRAMEWORK_PATH} = undef; },
    argName => "DYLD_FRAMEWORK_PATH",
    argValue => "/System/Library/PrivateFrameworks",
    expectedValue => "/System/Library/PrivateFrameworks",
    description => "Append to undefined variable",
},
{
    setup => sub { $ENV{DYLD_FRAMEWORK_PATH} = ""; },
    argName => "DYLD_FRAMEWORK_PATH",
    argValue => "/System/Library/PrivateFrameworks",
    expectedValue => join($Config{path_sep}, "", "/System/Library/PrivateFrameworks"),
    description => "Append to empty path",
},
{
    setup => sub { $ENV{DYLD_FRAMEWORK_PATH} = $Config{path_sep}; },
    argName => "DYLD_FRAMEWORK_PATH",
    argValue => "/System/Library/PrivateFrameworks",
    expectedValue => join($Config{path_sep}, "", "", "/System/Library/PrivateFrameworks"),
    description => "Append to empty path with separator",
},
{
    setup => sub { $ENV{DYLD_FRAMEWORK_PATH} = "/System/Library/Frameworks"; },
    argName => "DYLD_FRAMEWORK_PATH",
    argValue => "/System/Library/PrivateFrameworks",
    expectedValue => join($Config{path_sep}, "/System/Library/Frameworks", "/System/Library/PrivateFrameworks"),
    description => "Append to single path with no separator",
},
{
    setup => sub { $ENV{DYLD_FRAMEWORK_PATH} = "/System/Library/Frameworks" . $Config{path_sep}; },
    argName => "DYLD_FRAMEWORK_PATH",
    argValue => "/System/Library/PrivateFrameworks",
    expectedValue => join($Config{path_sep}, "/System/Library/Frameworks", "", "/System/Library/PrivateFrameworks"),
    description => "Append to single path with separator",
},
{
    setup => sub { $ENV{DYLD_FRAMEWORK_PATH} = "/System/Library/Frameworks" . $Config{path_sep} . "/System/Library/PrivateFrameworks"; },
    argName => "DYLD_FRAMEWORK_PATH",
    argValue => "/System/Library/PrivateFrameworks",
    expectedValue => join($Config{path_sep}, "/System/Library/Frameworks", "/System/Library/PrivateFrameworks", "/System/Library/PrivateFrameworks"),
    description => "Append to multiple, duplicate path with no separator",
},
{
    setup => sub { $ENV{DYLD_FRAMEWORK_PATH} = "/System/Library/Frameworks" . $Config{path_sep} . "/System/Library/PrivateFrameworks" . $Config{path_sep}; },
    argName => "DYLD_FRAMEWORK_PATH",
    argValue => "/System/Library/PrivateFrameworks",
    expectedValue => join($Config{path_sep}, "/System/Library/Frameworks", "/System/Library/PrivateFrameworks", "", "/System/Library/PrivateFrameworks"),
    description => "Append to multiple, duplicate path with separator",
},
);

plan(tests => 2 * @testCases);

foreach my $testCase (@testCases) {
    local %ENV;
    $testCase->{setup}->();
    my $result = appendToEnvironmentVariableList($testCase->{argName}, $testCase->{argValue});
    is_deeply($result, $testCase->{expectedValue}, "appendToEnvironmentVariableList: $testCase->{description} - return result");
    is_deeply($testCase->{expectedValue}, $ENV{$testCase->{argName}}, "appendToEnvironmentVariableList: $testCase->{description} - environment variable value");
}

