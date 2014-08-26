#!/usr/bin/perl -w

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

use strict;
use warnings;

use File::Spec;
use File::Temp qw/ tempdir /;

use Test::More;

my @testCases = 
(
    {
        'RC_ProjectSourceVersion' => '5300.4.3.2.1',
        'RC_PROJECTBUILDVERSION' => undef,
        'expectedVersionResult' => '5300.4003.2001',
        'expectedBuildVersionResult' => 1,
    },

    {
        'RC_ProjectSourceVersion' => '5300.4.3.2.1',
        'RC_PROJECTBUILDVERSION' => 156,
        'expectedVersionResult' => '5300.4003.2001',
        'expectedBuildVersionResult' => 156,
    },
);

# This test should only be run on Windows
if ($^O ne 'MSWin32' && $^O ne 'cygwin') {
    plan(tests => 1);
    is(1, 1, 'do nothing for non-Windows builds.');
    exit 0;    
}

my $testCasesCount = scalar(@testCases) * 2;
plan(tests => $testCasesCount);

my $TOOLS_PATH = $ENV{'WEBKIT_LIBRARIES'};
my $AUTO_VERSION_SCRIPT = File::Spec->catfile($TOOLS_PATH, 'tools', 'scripts', 'auto-version.pl');
my $VERSION_STAMP_SCRIPT = File::Spec->catfile($TOOLS_PATH, 'tools', 'scripts', 'version-stamp.pl');

foreach my $testCase (@testCases) {
    my $testOutputDir = tempdir(CLEANUP => 1);
    `RC_ProjectSourceVersion="$testCase->{'RC_ProjectSourceVersion'}" perl $AUTO_VERSION_SCRIPT $testOutputDir`;

    my $command;
    if (defined($testCase->{'RC_PROJECTBUILDVERSION'})) {
        $command="RC_PROJECTBUILDVERSION=\"$testCase->{'RC_PROJECTBUILDVERSION'}\" ";
    }
    $command .= "perl $VERSION_STAMP_SCRIPT $testOutputDir $testOutputDir";

    my @versionStamperOutput = qx($command 2>&1);

    foreach my $line (@versionStamperOutput) {
        if ($line !~ m/RC_PROJECTBUILDVERSION/) {
            next;
        }

        chomp($line);

        $line =~ m/RC_PROJECTSOURCEVERSION=([^\s]+) and RC_PROJECTBUILDVERSION=(.*)$/;

        my $projectSourceResult=$1;
        my $buildVersionResult=$2;

        is($projectSourceResult, $testCase->{expectedVersionResult}, "$testCase->{'RC_ProjectSourceVersion'}: $testCase->{expectedVersionResult}");

        my $testCaseInput = $testCase->{'RC_PROJECTBUILDVERSION'} || 'undefined';
        is($buildVersionResult, $testCase->{expectedBuildVersionResult}, "$testCaseInput: $testCase->{expectedBuildVersionResult}");
    }
}
