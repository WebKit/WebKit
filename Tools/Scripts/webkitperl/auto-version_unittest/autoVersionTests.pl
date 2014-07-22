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
        expectedResults => {
            '__VERSION_TEXT__' => '300.4.3',
            '__BUILD_NUMBER__' => '300.4.3',
            '__BUILD_NUMBER_SHORT__' => '300.4.3',
            '__VERSION_MAJOR__' => '3',
            '__VERSION_MINOR__' => '00',
            '__VERSION_TINY__' => '4',
            '__VERSION_BUILD__' => '3',
            '__BUILD_NUMBER_MAJOR__' => '300',
            '__BUILD_NUMBER_MINOR__' => '4',
            '__BUILD_NUMBER_VARIANT__' => '3',
        },
    },

    {
        'RC_ProjectSourceVersion' => '530.4.3.2.1',
        expectedResults => {
            '__VERSION_TEXT__' => '530.4.3',
            '__BUILD_NUMBER__' => '530.4.3',
            '__BUILD_NUMBER_SHORT__' => '530.4.3',
            '__VERSION_MAJOR__' => '5',
            '__VERSION_MINOR__' => '30',
            '__VERSION_TINY__' => '4',
            '__VERSION_BUILD__' => '3',
            '__BUILD_NUMBER_MAJOR__' => '530',
            '__BUILD_NUMBER_MINOR__' => '4',
            '__BUILD_NUMBER_VARIANT__' => '3',
        },
    },

    {
        'RC_ProjectSourceVersion' => '53.4.3.2.1',
        expectedResults => {
            '__VERSION_TEXT__' => '53.4.3',
            '__BUILD_NUMBER__' => '53.4.3',
            '__BUILD_NUMBER_SHORT__' => '53.4.3',
            '__VERSION_MAJOR__' => '5',
            '__VERSION_MINOR__' => '3',
            '__VERSION_TINY__' => '4',
            '__VERSION_BUILD__' => '3',
            '__BUILD_NUMBER_MAJOR__' => '53',
            '__BUILD_NUMBER_MINOR__' => '4',
            '__BUILD_NUMBER_VARIANT__' => '3',
        },
    },

    {
        'RC_ProjectSourceVersion' => '5.4.3.2.1',
        expectedResults => {
            '__VERSION_TEXT__' => '5.4.3',
            '__BUILD_NUMBER__' => '5.4.3',
            '__BUILD_NUMBER_SHORT__' => '5.4.3',
            '__VERSION_MAJOR__' => '5',
            '__VERSION_MINOR__' => '',
            '__VERSION_TINY__' => '4',
            '__VERSION_BUILD__' => '3',
            '__BUILD_NUMBER_MAJOR__' => '5',
            '__BUILD_NUMBER_MINOR__' => '4',
            '__BUILD_NUMBER_VARIANT__' => '3',
        },
    },

    {
        'RC_ProjectSourceVersion' => '5300.4.3.2',
        expectedResults => {
            '__VERSION_TEXT__' => '300.4.3',
            '__BUILD_NUMBER__' => '300.4.3',
            '__BUILD_NUMBER_SHORT__' => '300.4.3',
            '__VERSION_MAJOR__' => '3',
            '__VERSION_MINOR__' => '00',
            '__VERSION_TINY__' => '4',
            '__VERSION_BUILD__' => '3',
            '__BUILD_NUMBER_MAJOR__' => '300',
            '__BUILD_NUMBER_MINOR__' => '4',
            '__BUILD_NUMBER_VARIANT__' => '3',
        },
    },

    {
        'RC_ProjectSourceVersion' => '530.4.3.2',
        expectedResults => {
            '__VERSION_TEXT__' => '530.4.3',
            '__BUILD_NUMBER__' => '530.4.3',
            '__BUILD_NUMBER_SHORT__' => '530.4.3',
            '__VERSION_MAJOR__' => '5',
            '__VERSION_MINOR__' => '30',
            '__VERSION_TINY__' => '4',
            '__VERSION_BUILD__' => '3',
            '__BUILD_NUMBER_MAJOR__' => '530',
            '__BUILD_NUMBER_MINOR__' => '4',
            '__BUILD_NUMBER_VARIANT__' => '3',
        },
    },

    {
        'RC_ProjectSourceVersion' => '53.4.3.2',
        expectedResults => {
            '__VERSION_TEXT__' => '53.4.3',
            '__BUILD_NUMBER__' => '53.4.3',
            '__BUILD_NUMBER_SHORT__' => '53.4.3',
            '__VERSION_MAJOR__' => '5',
            '__VERSION_MINOR__' => '3',
            '__VERSION_TINY__' => '4',
            '__VERSION_BUILD__' => '3',
            '__BUILD_NUMBER_MAJOR__' => '53',
            '__BUILD_NUMBER_MINOR__' => '4',
            '__BUILD_NUMBER_VARIANT__' => '3',
        },
    },

    {
        'RC_ProjectSourceVersion' => '5.4.3.2',
        expectedResults => {
            '__VERSION_TEXT__' => '5.4.3',
            '__BUILD_NUMBER__' => '5.4.3',
            '__BUILD_NUMBER_SHORT__' => '5.4.3',
            '__VERSION_MAJOR__' => '5',
            '__VERSION_MINOR__' => '',
            '__VERSION_TINY__' => '4',
            '__VERSION_BUILD__' => '3',
            '__BUILD_NUMBER_MAJOR__' => '5',
            '__BUILD_NUMBER_MINOR__' => '4',
            '__BUILD_NUMBER_VARIANT__' => '3',
        },
    },

    {
        'RC_ProjectSourceVersion' => '5300.4.3',
        expectedResults => {
            '__VERSION_TEXT__' => '300.4.3',
            '__BUILD_NUMBER__' => '300.4.3',
            '__BUILD_NUMBER_SHORT__' => '300.4.3',
            '__VERSION_MAJOR__' => '3',
            '__VERSION_MINOR__' => '00',
            '__VERSION_TINY__' => '4',
            '__VERSION_BUILD__' => '3',
            '__BUILD_NUMBER_MAJOR__' => '300',
            '__BUILD_NUMBER_MINOR__' => '4',
            '__BUILD_NUMBER_VARIANT__' => '3',
        },
    },

    {
        'RC_ProjectSourceVersion' => '530.4.3',
        expectedResults => {
            '__VERSION_TEXT__' => '530.4.3',
            '__BUILD_NUMBER__' => '530.4.3',
            '__BUILD_NUMBER_SHORT__' => '530.4.3',
            '__VERSION_MAJOR__' => '5',
            '__VERSION_MINOR__' => '30',
            '__VERSION_TINY__' => '4',
            '__VERSION_BUILD__' => '3',
            '__BUILD_NUMBER_MAJOR__' => '530',
            '__BUILD_NUMBER_MINOR__' => '4',
            '__BUILD_NUMBER_VARIANT__' => '3',
        },
    },

    {
        'RC_ProjectSourceVersion' => '53.4.3',
        expectedResults => {
            '__VERSION_TEXT__' => '53.4.3',
            '__BUILD_NUMBER__' => '53.4.3',
            '__BUILD_NUMBER_SHORT__' => '53.4.3',
            '__VERSION_MAJOR__' => '5',
            '__VERSION_MINOR__' => '3',
            '__VERSION_TINY__' => '4',
            '__VERSION_BUILD__' => '3',
            '__BUILD_NUMBER_MAJOR__' => '53',
            '__BUILD_NUMBER_MINOR__' => '4',
            '__BUILD_NUMBER_VARIANT__' => '3',
        },
    },

    {
        'RC_ProjectSourceVersion' => '5.4.3',
        expectedResults => {
            '__VERSION_TEXT__' => '5.4.3',
            '__BUILD_NUMBER__' => '5.4.3',
            '__BUILD_NUMBER_SHORT__' => '5.4.3',
            '__VERSION_MAJOR__' => '5',
            '__VERSION_MINOR__' => '',
            '__VERSION_TINY__' => '4',
            '__VERSION_BUILD__' => '3',
            '__BUILD_NUMBER_MAJOR__' => '5',
            '__BUILD_NUMBER_MINOR__' => '4',
            '__BUILD_NUMBER_VARIANT__' => '3',
        },
    },

    {
        'RC_ProjectSourceVersion' => '5300.4',
        expectedResults => {
            '__VERSION_TEXT__' => '300.4.0',
            '__BUILD_NUMBER__' => '300.4.0',
            '__BUILD_NUMBER_SHORT__' => '300.4.0',
            '__VERSION_MAJOR__' => '3',
            '__VERSION_MINOR__' => '00',
            '__VERSION_TINY__' => '4',
            '__VERSION_BUILD__' => '0',
            '__BUILD_NUMBER_MAJOR__' => '300',
            '__BUILD_NUMBER_MINOR__' => '4',
            '__BUILD_NUMBER_VARIANT__' => '0',
        },
    },

    {
        'RC_ProjectSourceVersion' => '530.4',
        expectedResults => {
            '__VERSION_TEXT__' => '530.4.0',
            '__BUILD_NUMBER__' => '530.4.0',
            '__BUILD_NUMBER_SHORT__' => '530.4.0',
            '__VERSION_MAJOR__' => '5',
            '__VERSION_MINOR__' => '30',
            '__VERSION_TINY__' => '4',
            '__VERSION_BUILD__' => '0',
            '__BUILD_NUMBER_MAJOR__' => '530',
            '__BUILD_NUMBER_MINOR__' => '4',
            '__BUILD_NUMBER_VARIANT__' => '0',
        },
    },

    {
        'RC_ProjectSourceVersion' => '53.4',
        expectedResults => {
            '__VERSION_TEXT__' => '53.4.0',
            '__BUILD_NUMBER__' => '53.4.0',
            '__BUILD_NUMBER_SHORT__' => '53.4.0',
            '__VERSION_MAJOR__' => '5',
            '__VERSION_MINOR__' => '3',
            '__VERSION_TINY__' => '4',
            '__VERSION_BUILD__' => '0',
            '__BUILD_NUMBER_MAJOR__' => '53',
            '__BUILD_NUMBER_MINOR__' => '4',
            '__BUILD_NUMBER_VARIANT__' => '0',
        },
    },

    {
        'RC_ProjectSourceVersion' => '5.4',
        expectedResults => {
            '__VERSION_TEXT__' => '5.4.0',
            '__BUILD_NUMBER__' => '5.4.0',
            '__BUILD_NUMBER_SHORT__' => '5.4.0',
            '__VERSION_MAJOR__' => '5',
            '__VERSION_MINOR__' => '',
            '__VERSION_TINY__' => '4',
            '__VERSION_BUILD__' => '0',
            '__BUILD_NUMBER_MAJOR__' => '5',
            '__BUILD_NUMBER_MINOR__' => '4',
            '__BUILD_NUMBER_VARIANT__' => '0',
        },
    },

    {
        'RC_ProjectSourceVersion' => '5300',
        expectedResults => {
            '__VERSION_TEXT__' => '300.0.0',
            '__BUILD_NUMBER__' => '300.0.0',
            '__BUILD_NUMBER_SHORT__' => '300.0.0',
            '__VERSION_MAJOR__' => '3',
            '__VERSION_MINOR__' => '00',
            '__VERSION_TINY__' => '0',
            '__VERSION_BUILD__' => '0',
            '__BUILD_NUMBER_MAJOR__' => '300',
            '__BUILD_NUMBER_MINOR__' => '0',
            '__BUILD_NUMBER_VARIANT__' => '0',
        },
    },

    {
        'RC_ProjectSourceVersion' => '530',
        expectedResults => {
            '__VERSION_TEXT__' => '530.0.0',
            '__BUILD_NUMBER__' => '530.0.0',
            '__BUILD_NUMBER_SHORT__' => '530.0.0',
            '__VERSION_MAJOR__' => '5',
            '__VERSION_MINOR__' => '30',
            '__VERSION_TINY__' => '0',
            '__VERSION_BUILD__' => '0',
            '__BUILD_NUMBER_MAJOR__' => '530',
            '__BUILD_NUMBER_MINOR__' => '0',
            '__BUILD_NUMBER_VARIANT__' => '0',
        },
    },

    # Smallest "Valid" case
    {
        'RC_ProjectSourceVersion' => '53',
        expectedResults => {
            '__VERSION_TEXT__' => '53.0.0',
            '__BUILD_NUMBER__' => '53.0.0',
            '__BUILD_NUMBER_SHORT__' => '53.0.0',
            '__VERSION_MAJOR__' => '5',
            '__VERSION_MINOR__' => '3',
            '__VERSION_TINY__' => '0',
            '__VERSION_BUILD__' => '0',
            '__BUILD_NUMBER_MAJOR__' => '53',
            '__BUILD_NUMBER_MINOR__' => '0',
            '__BUILD_NUMBER_VARIANT__' => '0',
        },
    },

    # We don't support 1-digit versions, but we should run without crashing
    {
        'RC_ProjectSourceVersion' => '5',
        expectedResults => {
            '__VERSION_TEXT__' => '5.0.0',
            '__BUILD_NUMBER__' => '5.0.0',
            '__BUILD_NUMBER_SHORT__' => '5.0.0',
            '__VERSION_MAJOR__' => '5',
            '__VERSION_MINOR__' => '',
            '__VERSION_TINY__' => '0',
            '__VERSION_BUILD__' => '0',
            '__BUILD_NUMBER_MAJOR__' => '5',
            '__BUILD_NUMBER_MINOR__' => '0',
            '__BUILD_NUMBER_VARIANT__' => '0',
        },
    },

    # Largest specified version test
    {
        'RC_ProjectSourceVersion' => '214747.99.99.99.99',
        expectedResults => {
            '__VERSION_TEXT__' => '747.99.99',
            '__BUILD_NUMBER__' => '747.99.99',
            '__BUILD_NUMBER_SHORT__' => '747.99.99',
            '__VERSION_MAJOR__' => '7',
            '__VERSION_MINOR__' => '47',
            '__VERSION_TINY__' => '99',
            '__VERSION_BUILD__' => '99',
            '__BUILD_NUMBER_MAJOR__' => '747',
            '__BUILD_NUMBER_MINOR__' => '99',
            '__BUILD_NUMBER_VARIANT__' => '99',
        },
    },
);

# This test should only be run on Windows
if ($^O ne 'MSWin32') {
    plan(tests => 1);
    is(1, 1, 'do nothing for non-Windows builds.');
    exit 0;    
}

my $testCasesCount = scalar(@testCases) * 10; # 10 expected results
plan(tests => $testCasesCount);

foreach my $testCase (@testCases) {
    my $toolsPath = $ENV{'WEBKIT_LIBRARIES'};
    my $autoVersionScript = File::Spec->catfile($toolsPath, 'tools', 'scripts', 'auto-version.pl');
    my $testOutputDir = tempdir(CLEANUP => 1);
    `RC_ProjectSourceVersion=$testCase->{'RC_ProjectSourceVersion'} perl $autoVersionScript $testOutputDir`;

    my $expectedResults = $testCase->{expectedResults};

    my $outputFile = File::Spec->catfile($testOutputDir, 'include', 'autoversion.h');
    open(TEST_OUTPUT, '<', $outputFile) or die "Unable to open $outputFile";

    while (my $line = <TEST_OUTPUT>) {
        foreach my $expectedResultKey (keys $expectedResults) {
            if ($line !~ m/$expectedResultKey/) {
                next;
            }

            $line =~ s/#define $expectedResultKey//;
            $line =~ s/^\s*(.*)\s*$/$1/;
            $line =~ s/^"(.*)"$/$1/;
            chomp($line);

            my $expectedResultValue = $expectedResults->{$expectedResultKey};
            is($line, $expectedResultValue, "$testCase->{'RC_ProjectSourceVersion'}: $expectedResultKey");
        }
    }
}
