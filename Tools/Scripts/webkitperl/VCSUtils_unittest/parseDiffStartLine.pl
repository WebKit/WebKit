#!/usr/bin/env perl

# Copyright (C) 2018 Apple Inc.  All rights reserved.
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

# Unit tests for parseDiffStartLine(), parseGitDiffStartLine(), parseSvnDiffStartLine().

use strict;
use warnings;

use constant {
    CR => "\r",
    CRLF => "\r\n",
    LF => "\n",
};

use Test::More;
use VCSUtils;

my @testCases = (
{
    testName => "Git diff start line",
    input => "diff --git a/Source/Makefile.shared b/Source/Makefile.shared",
    expected => "Source/Makefile.shared",
    isGitDiffStartLine => 1,
    isValid => 1,
},
{
    testName => "Git diff start line with --no-prefix",
    input => "diff --git Source/Makefile.shared Source/Makefile.shared",
    expected => "Source/Makefile.shared",
    isGitDiffStartLine => 1,
    isValid => 1,
},
{
    testName => "Git diff start line with space in path",
    input => "diff --git a/LayoutTests/http/tests/misc/resources/a success.html b/LayoutTests/http/tests/misc/resources/a success.html",
    expected => "LayoutTests/http/tests/misc/resources/a success.html",
    isGitDiffStartLine => 1,
    isValid => 1,
},
{
    testName => "Invalid Git diff start line",
    input => "===================================================================",
    expected => undef,
    isGitDiffStartLine => 1,
    isValid => 0,
},
{
    testName => "Svn diff start line",
    input => "Index: Source/Makefile.shared",
    expected => "Source/Makefile.shared",
    isSvnDiffStartLine => 1,
    isValid => 1,
},
{
    testName => "Svn diff start line with space in path",
    input => "Index: LayoutTests/http/tests/misc/resources/a success.html",
    expected => "LayoutTests/http/tests/misc/resources/a success.html",
    isSvnDiffStartLine => 1,
    isValid => 1,
},
{
    testName => "Invalid Svn diff start line",
    input => "===================================================================",
    expected => undef,
    isSvnDiffStartLine => 1,
    isValid => 0,
},
);

my $testCaseCount = scalar @testCases;
plan(tests => 9 * $testCaseCount);

foreach my $testCase (@testCases) {
    # Make sure future modifications or new test cases are valid.
    ok(exists $testCase->{expected}, "'expected' key exists: $testCase->{testName}");
    ok(exists $testCase->{input}, "'input' key exists: $testCase->{testName}");
    ok(exists $testCase->{isGitDiffStartLine} ^ exists $testCase->{isSvnDiffStartLine}, "Exactly one of 'isGitDiffStartLine' key or 'isSvnDiffStartLine' key exists: $testCase->{testName}");
    ok(exists $testCase->{isValid}, "'isValid' key exists: $testCase->{testName}");
    ok(exists $testCase->{testName}, "'testName' key exists");

    my $testCaseName = "parseDiffStartLine(): $testCase->{testName}";
    my $got = VCSUtils::parseDiffStartLine($testCase->{input});
    is($got, $testCase->{expected}, $testCaseName);

    if ($testCase->{isGitDiffStartLine}) {
        SKIP: {
            skip "parseGitDiffStartLine() does not handle invalid input", 3 if !$testCase->{isValid};

            $testCaseName = "parseGitDiffStartLine(): CR line ending: $testCase->{testName}";
            my $input = $testCase->{input} . CR;
            $got = VCSUtils::parseGitDiffStartLine($input);
            is($got, $testCase->{expected}, $testCaseName);

            $testCaseName = "parseGitDiffStartLine(): CRLF line ending: $testCase->{testName}";
            $input = $testCase->{input} . CRLF;
            $got = VCSUtils::parseGitDiffStartLine($input);
            is($got, $testCase->{expected}, $testCaseName);

            $testCaseName = "parseGitDiffStartLine(): LF line ending: $testCase->{testName}";
            $input = $testCase->{input} . LF;
            $got = VCSUtils::parseGitDiffStartLine($input);
            is($got, $testCase->{expected}, $testCaseName);
        }
    } elsif ($testCase->{isSvnDiffStartLine}) {
        $testCaseName = "parseSvnDiffStartLine(): CR line ending: $testCase->{testName}";
        my $input = $testCase->{input} . CR;
        $got = VCSUtils::parseSvnDiffStartLine($input);
        is($got, $testCase->{expected}, $testCaseName);

        $testCaseName = "parseSvnDiffStartLine(): CRLF line ending: $testCase->{testName}";
        $input = $testCase->{input} . CRLF;
        $got = VCSUtils::parseSvnDiffStartLine($input);
        is($got, $testCase->{expected}, $testCaseName);

        $testCaseName = "parseSvnDiffStartLine(): LF line ending: $testCase->{testName}";
        $input = $testCase->{input} . LF;
        $got = VCSUtils::parseSvnDiffStartLine($input);
        is($got, $testCase->{expected}, $testCaseName);
    } else {
        fail("CR line ending: isGitDiffStartLine or isSvnDiffStartLine is not set for $testCase->{testName}");
        fail("CFLF line ending: isGitDiffStartLine or isSvnDiffStartLine is not set for $testCase->{testName}");
        fail("FL line ending: isGitDiffStartLine or isSvnDiffStartLine is not set for $testCase->{testName}");
    }
}
