#!/usr/bin/env perl

# Copyright (C) 2018 Bocoup LLC. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above
#    copyright notice, this list of conditions and the following
#    disclaimer.
# 2. Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials
#    provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

use strict;
use warnings;

use Test::More;
use File::Spec;
use FindBin;
use Cwd qw(abs_path);
use File::Path qw(rmtree);
use File::Temp qw(tempfile);
use File::Compare qw(compare);
use webkitdirs qw(isWindows isCygwin isWinCairo);

# The test262-harness script is not written for compatiblity with Windows
if (isWindows() || isCygwin() || isWinCairo()) {
    plan(tests => 1);
    is(1, 1, 'do nothing for Windows builds.');
    exit 0;
}

my $Bin = $FindBin::Bin;
my $runner = abs_path("$Bin/../../test262-runner -p 1");
my $mockTest262 = abs_path("$Bin/fixtures");


# testCase format
#  TESTNAME                  - Description of test case
#  TEST262TESTS_ARG          - test262-runner CLI argument to specify mock
#                              test262 test file located in ./fixtures/test/..
#  EXPECTATION_ARG           - test262-runner CLI argument to specify mock
#                              expectations.yaml file in ./fixtures/
#  MOCK_JSC_ARG              - test262-runner CLI argument to specify mock jsc,
#                              located in ./fixtures/.., which mimics jsc output
#  EXITSTATUS                - Expected resulting exit code of test262-runner
#  EXPECTED_NEW_FAIL_COUNT   - Expected resulting fail count of test262-runner
#                              counts are occurances of "! NEW FAIL"

my @testCases = (
    {
        "TESTNAME" => "test262 test failed, ignore expectations",
        "TEST262TESTS_ARG" => "--test-only test/fail.js",
        "EXPECTATION_ARG" => "--ignore-expectations",
        "MOCK_JSC_ARG" => "--jsc $mockTest262/mock-jsc-fail.pl",
        "EXITSTATUS" => 1,
        "EXPECTED_NEW_FAIL_COUNT" => 2,
    },
    {
        "TESTNAME" => "test262 test passed, ignore expectations",
        "TEST262TESTS_ARG" => "--test-only test/pass.js",
        "EXPECTATION_ARG" => "--ignore-expectations",
        "MOCK_JSC_ARG" => "--jsc $mockTest262/mock-jsc-pass.pl",
        "EXITSTATUS" => 0,
        "EXPECTED_NEW_FAIL_COUNT" => 0,
    },
    {
        "TESTNAME" => "test262 tests newly failed",
        "TEST262TESTS_ARG" => "--test-only test/expected-to-pass-now-failing.js",
        "EXPECTATION_ARG" => "--expectations $Bin/fixtures/expectations.yaml",
        "MOCK_JSC_ARG" => "--jsc $mockTest262/mock-jsc-fail.pl",
        "EXITSTATUS" => 1,
        "EXPECTED_NEW_FAIL_COUNT" => 2,
    },
    {
        "TESTNAME" => "test262 tests newly passed",
        "TEST262TESTS_ARG" => "--test-only test/expected-to-fail-now-passing.js",
        "EXPECTATION_ARG" => "--expectations $Bin/fixtures/expectations.yaml",
        "MOCK_JSC_ARG" => "--jsc $mockTest262/mock-jsc-pass.pl",
        "EXITSTATUS" => 0,
        "EXPECTED_NEW_FAIL_COUNT" => 0,
    },
    {
        "TESTNAME" => "test262 tests fails, expected failure",
        "TEST262TESTS_ARG" => "--test-only  test/expected-to-fail-now-failing.js",
        "EXPECTATION_ARG" => "--expectations $Bin/fixtures/expectations.yaml",
        "MOCK_JSC_ARG" => "--jsc $mockTest262/mock-jsc-fail.pl",
        "EXITSTATUS" => 0,
        "EXPECTED_NEW_FAIL_COUNT" => 0,
    },
    {
        "TESTNAME" => "test262 tests fails, with unexpected error string",
        "TEST262TESTS_ARG" => "--test-only test/expected-to-fail-now-failing-with-new-error.js",
        "EXPECTATION_ARG" => "--expectations $Bin/fixtures/expectations.yaml",
        "MOCK_JSC_ARG" => "--jsc $mockTest262/mock-jsc-fail-new-error.pl",
        "EXITSTATUS" => 1,
        "EXPECTED_NEW_FAIL_COUNT" => 2,
    },
);

my $testCasesCount = (scalar(@testCases) * 2) + 1;
plan(tests => $testCasesCount);

## Test error codes and expected output messages of tests ##

foreach my $testCase (@testCases) {

    my $test = $testCase->{TEST262TESTS_ARG};
    my $expectation = $testCase->{EXPECTATION_ARG};
    my $jsc = $testCase->{MOCK_JSC_ARG};
    my $test262loc = "--t262 $mockTest262";

    my $cmd = qq($runner --release $jsc $test262loc $test $expectation);
    my $output = qx($cmd);

    # Test the resulting exit code
    my $exitCode = $? >> 8;
    my $expectedExitCode = $testCase->{EXITSTATUS};
    my $testName =  $testCase->{TESTNAME} . " (exit code: $expectedExitCode)";
    is($exitCode, $expectedExitCode, $testName);

    # Test the number of occurences of string "! NEW FAIL"
    my @newFailCount = $output =~ /! NEW FAIL/g;
    my $expectedNewFailures = $testCase->{EXPECTED_NEW_FAIL_COUNT};
    $testName =  $testCase->{TESTNAME} . " (new failures: $expectedNewFailures)";
    is(scalar(@newFailCount), $expectedNewFailures, $testName);
}

## Test format of saved expectations file ##

my $test262loc = "--t262 $mockTest262";
my ($expectationsFH, $expectationsFile) = tempfile();
my $expect = "--expectations $expectationsFile";
my $test = "--test-only test/fail.js";
my $jsc = "--jsc $mockTest262/mock-jsc-fail.pl";
my $cmd = qq($runner --save --ignore-expectations $jsc $test262loc $test $expect);
qx($cmd);

my $expectedExpectationsFile = "$Bin/fixtures/expectations-compare.yaml";
my $filesmatch = compare($expectationsFile, $expectedExpectationsFile);
ok($filesmatch == 0, "expectations yaml file format");

close $expectationsFH;

END {
    # Clean up test262 results directory after running tests
    my $resultsDir = $ENV{PWD} . "/test262-results";
    if (-e $resultsDir) {
        rmtree($resultsDir);
    }
}
