#!/usr/bin/env perl
#
# Copyright (C) 2026 Apple Inc. All rights reserved.
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

# Continuation lines line up under the first test name, past the 15 character
# "        Tests: " lead string.
my $multipleIndent = " " x 15;

my @testCaseHashRefs = (
{
    testName => "No tests",
    inputTests => [],
    expected => ""
},
{
    testName => "Single layout test drops the LayoutTests prefix",
    inputTests => ["LayoutTests/fast/dom/added-test.html"],
    expected => "        Test: fast/dom/added-test.html\n\n"
},
{
    testName => "Single API test keeps its full path",
    inputTests => ["Tools/TestWebKitAPI/Tests/WebKit/AddedTest.mm"],
    expected => "        Test: Tools/TestWebKitAPI/Tests/WebKit/AddedTest.mm\n\n"
},
{
    testName => "Multiple layout tests are sorted",
    inputTests => [
        "LayoutTests/fast/dom/zebra.html",
        "LayoutTests/fast/dom/apple.html",
        "LayoutTests/fast/css/middle.html",
    ],
    expected => "        Tests: fast/css/middle.html\n"
        . $multipleIndent . "fast/dom/apple.html\n"
        . $multipleIndent . "fast/dom/zebra.html\n"
        . "\n"
},
{
    # Sorting the full paths put every Tools test after every LayoutTests test,
    # so the printed list was not alphabetical once the prefix was stripped.
    testName => "Layout and API tests are sorted as printed",
    inputTests => [
        "LayoutTests/fast/dom/added-test.html",
        "Tools/TestWebKitAPI/Tests/WebKit/AddedTest.mm",
        "LayoutTests/http/tests/added-test.html",
    ],
    expected => "        Tests: Tools/TestWebKitAPI/Tests/WebKit/AddedTest.mm\n"
        . $multipleIndent . "fast/dom/added-test.html\n"
        . $multipleIndent . "http/tests/added-test.html\n"
        . "\n"
},
{
    testName => "Test path containing a space",
    inputTests => ["Tools/TestWebKitAPI/Tests/WebKit Swift/WebPageTests.swift"],
    expected => "        Test: Tools/TestWebKitAPI/Tests/WebKit Swift/WebPageTests.swift\n\n"
},
{
    testName => "LayoutTests prefix is only stripped from the start of a path",
    inputTests => ["Source/WebCore/LayoutTests/not-a-prefix.html"],
    expected => "        Test: Source/WebCore/LayoutTests/not-a-prefix.html\n\n"
},
);

my $testCasesCount = @testCaseHashRefs;
plan(tests => $testCasesCount);

foreach my $testCase (@testCaseHashRefs) {
    my $got = PrepareChangeLog::testListForChangeLog(@{$testCase->{inputTests}});
    is($got, $testCase->{expected}, $testCase->{testName});
}
