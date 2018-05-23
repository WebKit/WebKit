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

# This test should not be run on Windows
if ($^O eq 'MSWin32' || $^O eq 'cygwin' || $^O eq 'WinCairo') {
    plan(tests => 1);
    is(1, 1, 'do nothing for Windows builds.');
    exit 0;
}

my $Bin = $FindBin::Bin;
my $runner = abs_path("$Bin/../../test262-runner");
my $mockTest262 = abs_path("$Bin/fixtures");

my @testCases = (
    {
        "TESTNAME" => "test262 test failed, ignore expectations",
        "EXITSTATUS" => 1,
        "TEST262TESTS_ARG" => "--test-only test/fail.js",
        "EXPECTATION_ARG" => "--ignore-expectations",
        "EXPECTED_NEW_FAIL_COUNT" => 2,
    },
    {
        "TESTNAME" => "test262 test passed, ignore expectations",
        "EXITSTATUS" => 0,
        "TEST262TESTS_ARG" => "--test-only test/pass.js",
        "EXPECTATION_ARG" => "--ignore-expectations",
        "EXPECTED_NEW_FAIL_COUNT" => 0,
    },
    {
        "TESTNAME" => "test262 tests newly failed",
        "EXITSTATUS" => 1,
        "TEST262TESTS_ARG" => "--test-only test/expected-to-pass-now-failing.js",
        "EXPECTATION_ARG" => "--expectations $Bin/fixtures/expectations.yaml",
        "EXPECTED_NEW_FAIL_COUNT" => 2,
    },
    {
        "TESTNAME" => "test262 tests newly passed",
        "EXITSTATUS" => 0,
        "TEST262TESTS_ARG" => "--test-only test/expected-to-fail-now-passing.js",
        "EXPECTATION_ARG" => "--expectations $Bin/fixtures/expectations.yaml",
        "EXPECTED_NEW_FAIL_COUNT" => 0,
    },
    {
        "TESTNAME" => "test262 tests fails, expected failure",
        "EXITSTATUS" => 0,
        "TEST262TESTS_ARG" => "--test-only  test/expected-to-fail-now-failing.js",
        "EXPECTATION_ARG" => "--expectations $Bin/fixtures/expectations.yaml",
        "EXPECTED_NEW_FAIL_COUNT" => 0,
    },
    {
        "TESTNAME" => "test262 tests fails, with unexpected error string",
        "EXITSTATUS" => 1,
        "TEST262TESTS_ARG" => "--test-only test/expected-to-fail-now-failing-with-new-error.js",
        "EXPECTATION_ARG" => "--expectations $Bin/fixtures/expectations.yaml",
        "EXPECTED_NEW_FAIL_COUNT" => 2,
    },
);

my $testCasesCount = (scalar(@testCases) * 2) + 1;
plan(tests => $testCasesCount);

## Test error codes and expected output messages of tests ##

foreach my $testcase (@testCases) {

    my $test = $testcase->{TEST262TESTS_ARG};
    my $expectation = $testcase->{EXPECTATION_ARG};
    my $test262loc = "--t262 $mockTest262";

    my $cmd = qq($runner $test262loc $test $expectation);
    my $output = qx($cmd);

    # Test the resulting exit code
    my $exitcode = $? >> 8;
    my $expectedexitcode = $testcase->{EXITSTATUS};
    my $testname =  $testcase->{TESTNAME} . " (exit code: $expectedexitcode)";
    is($exitcode, $testcase->{EXITSTATUS}, $testname);

    # Test the number of occurences of string "! NEW FAIL"
    my @newfailcount = $output =~ /! NEW FAIL/g;
    my $expectednewfailures = $testcase->{EXPECTED_NEW_FAIL_COUNT};
    $testname =  $testcase->{TESTNAME} . " (new failures: $expectednewfailures)";
    is(scalar(@newfailcount), $expectednewfailures, $testname);
}

## Test format of saved expectations file ##

my $test262loc = "--t262 $mockTest262";
my ($expectationsfh, $expectationsfile) = tempfile();
my $expect = "--expectations $expectationsfile";
my $test = "--test-only test/fail.js";
my $cmd = qq($runner --save --ignore-expectations $test262loc $test $expect);
qx($cmd);

my $expectedexpectationsfile = "$Bin/fixtures/expectations-compare.yaml";
my $filesmatch = compare($expectationsfile, $expectedexpectationsfile);
ok($filesmatch == 0, "expectations yaml file format");

close $expectationsfh;

END {
    # Clean up test262 results directory after running tests
    my $resultsDir = $ENV{PWD} . "/test262-results";
    if (-e $resultsDir) {
        rmtree($resultsDir);
    }
}
