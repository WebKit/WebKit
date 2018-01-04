#!/usr/bin/env perl

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
            '__VERSION_TEXT__' => '300.4.3.2.1',
            '__BUILD_NUMBER__' => '300.4.3.2.1',
            '__BUILD_NUMBER_SHORT__' => '300.4.3.2.1',
            '__VERSION_MAJOR__' => '300',
            '__VERSION_MINOR__' => '4003',
            '__VERSION_TINY__' => '2001',
            '__VERSION_BUILD__' => '1',
            '__BUILD_NUMBER_MAJOR__' => '300',
            '__BUILD_NUMBER_MINOR__' => '4003',
            '__BUILD_NUMBER_VARIANT__' => '2001',
            '__FULL_BUILD_MAJOR_VERSION__' => '5300',
        },
    },

    {
        'RC_ProjectSourceVersion' => '530.4.3.2.1',
        expectedResults => {
            '__VERSION_TEXT__' => '530.4.3.2.1',
            '__BUILD_NUMBER__' => '530.4.3.2.1',
            '__BUILD_NUMBER_SHORT__' => '530.4.3.2.1',
            '__VERSION_MAJOR__' => '530',
            '__VERSION_MINOR__' => '4003',
            '__VERSION_TINY__' => '2001',
            '__VERSION_BUILD__' => '1',
            '__BUILD_NUMBER_MAJOR__' => '530',
            '__BUILD_NUMBER_MINOR__' => '4003',
            '__BUILD_NUMBER_VARIANT__' => '2001',
            '__FULL_BUILD_MAJOR_VERSION__' => '530',
        },
    },

    {
        'RC_ProjectSourceVersion' => '53.4.3.2.1',
        expectedResults => {
            '__VERSION_TEXT__' => '53.4.3.2.1',
            '__BUILD_NUMBER__' => '53.4.3.2.1',
            '__BUILD_NUMBER_SHORT__' => '53.4.3.2.1',
            '__VERSION_MAJOR__' => '53',
            '__VERSION_MINOR__' => '4003',
            '__VERSION_TINY__' => '2001',
            '__VERSION_BUILD__' => '1',
            '__BUILD_NUMBER_MAJOR__' => '53',
            '__BUILD_NUMBER_MINOR__' => '4003',
            '__BUILD_NUMBER_VARIANT__' => '2001',
            '__FULL_BUILD_MAJOR_VERSION__' => '53',
        },
    },

    {
        'RC_ProjectSourceVersion' => '5.4.3.2.1',
        expectedResults => {
            '__VERSION_TEXT__' => '5.4.3.2.1',
            '__BUILD_NUMBER__' => '5.4.3.2.1',
            '__BUILD_NUMBER_SHORT__' => '5.4.3.2.1',
            '__VERSION_MAJOR__' => '5',
            '__VERSION_MINOR__' => '4003',
            '__VERSION_TINY__' => '2001',
            '__VERSION_BUILD__' => '1',
            '__BUILD_NUMBER_MAJOR__' => '5',
            '__BUILD_NUMBER_MINOR__' => '4003',
            '__BUILD_NUMBER_VARIANT__' => '2001',
            '__FULL_BUILD_MAJOR_VERSION__' => '5',
        },
    },

    {
        'RC_ProjectSourceVersion' => '5300.4.3.2',
        expectedResults => {
            '__VERSION_TEXT__' => '300.4.3.2',
            '__BUILD_NUMBER__' => '300.4.3.2',
            '__BUILD_NUMBER_SHORT__' => '300.4.3.2',
            '__VERSION_MAJOR__' => '300',
            '__VERSION_MINOR__' => '4003',
            '__VERSION_TINY__' => '2000',
            '__VERSION_BUILD__' => '2',
            '__BUILD_NUMBER_MAJOR__' => '300',
            '__BUILD_NUMBER_MINOR__' => '4003',
            '__BUILD_NUMBER_VARIANT__' => '2000',
            '__FULL_BUILD_MAJOR_VERSION__' => '5300',
        },
    },

    {
        'RC_ProjectSourceVersion' => '530.4.3.2',
        expectedResults => {
            '__VERSION_TEXT__' => '530.4.3.2',
            '__BUILD_NUMBER__' => '530.4.3.2',
            '__BUILD_NUMBER_SHORT__' => '530.4.3.2',
            '__VERSION_MAJOR__' => '530',
            '__VERSION_MINOR__' => '4003',
            '__VERSION_TINY__' => '2000',
            '__VERSION_BUILD__' => '2',
            '__BUILD_NUMBER_MAJOR__' => '530',
            '__BUILD_NUMBER_MINOR__' => '4003',
            '__BUILD_NUMBER_VARIANT__' => '2000',
            '__FULL_BUILD_MAJOR_VERSION__' => '530',
        },
    },

    {
        'RC_ProjectSourceVersion' => '53.4.3.2',
        expectedResults => {
            '__VERSION_TEXT__' => '53.4.3.2',
            '__BUILD_NUMBER__' => '53.4.3.2',
            '__BUILD_NUMBER_SHORT__' => '53.4.3.2',
            '__VERSION_MAJOR__' => '53',
            '__VERSION_MINOR__' => '4003',
            '__VERSION_TINY__' => '2000',
            '__VERSION_BUILD__' => '2',
            '__BUILD_NUMBER_MAJOR__' => '53',
            '__BUILD_NUMBER_MINOR__' => '4003',
            '__BUILD_NUMBER_VARIANT__' => '2000',
            '__FULL_BUILD_MAJOR_VERSION__' => '53',
        },
    },

    {
        'RC_ProjectSourceVersion' => '5.4.3.2',
        expectedResults => {
            '__VERSION_TEXT__' => '5.4.3.2',
            '__BUILD_NUMBER__' => '5.4.3.2',
            '__BUILD_NUMBER_SHORT__' => '5.4.3.2',
            '__VERSION_MAJOR__' => '5',
            '__VERSION_MINOR__' => '4003',
            '__VERSION_TINY__' => '2000',
            '__VERSION_BUILD__' => '2',
            '__BUILD_NUMBER_MAJOR__' => '5',
            '__BUILD_NUMBER_MINOR__' => '4003',
            '__BUILD_NUMBER_VARIANT__' => '2000',
            '__FULL_BUILD_MAJOR_VERSION__' => '5',
        },
    },

    {
        'RC_ProjectSourceVersion' => '5300.4.3',
        expectedResults => {
            '__VERSION_TEXT__' => '300.4.3',
            '__BUILD_NUMBER__' => '300.4.3',
            '__BUILD_NUMBER_SHORT__' => '300.4.3',
            '__VERSION_MAJOR__' => '300',
            '__VERSION_MINOR__' => '4003',
            '__VERSION_TINY__' => '0',
            '__VERSION_BUILD__' => '0',
            '__BUILD_NUMBER_MAJOR__' => '300',
            '__BUILD_NUMBER_MINOR__' => '4003',
            '__BUILD_NUMBER_VARIANT__' => '0',
            '__FULL_BUILD_MAJOR_VERSION__' => '5300',
        },
    },

    {
        'RC_ProjectSourceVersion' => '530.4.3',
        expectedResults => {
            '__VERSION_TEXT__' => '530.4.3',
            '__BUILD_NUMBER__' => '530.4.3',
            '__BUILD_NUMBER_SHORT__' => '530.4.3',
            '__VERSION_MAJOR__' => '530',
            '__VERSION_MINOR__' => '4003',
            '__VERSION_TINY__' => '0',
            '__VERSION_BUILD__' => '0',
            '__BUILD_NUMBER_MAJOR__' => '530',
            '__BUILD_NUMBER_MINOR__' => '4003',
            '__BUILD_NUMBER_VARIANT__' => '0',
            '__FULL_BUILD_MAJOR_VERSION__' => '530',
        },
    },

    {
        'RC_ProjectSourceVersion' => '53.4.3',
        expectedResults => {
            '__VERSION_TEXT__' => '53.4.3',
            '__BUILD_NUMBER__' => '53.4.3',
            '__BUILD_NUMBER_SHORT__' => '53.4.3',
            '__VERSION_MAJOR__' => '53',
            '__VERSION_MINOR__' => '4003',
            '__VERSION_TINY__' => '0',
            '__VERSION_BUILD__' => '0',
            '__BUILD_NUMBER_MAJOR__' => '53',
            '__BUILD_NUMBER_MINOR__' => '4003',
            '__BUILD_NUMBER_VARIANT__' => '0',
            '__FULL_BUILD_MAJOR_VERSION__' => '53',
        },
    },

    {
        'RC_ProjectSourceVersion' => '5.4.3',
        expectedResults => {
            '__VERSION_TEXT__' => '5.4.3',
            '__BUILD_NUMBER__' => '5.4.3',
            '__BUILD_NUMBER_SHORT__' => '5.4.3',
            '__VERSION_MAJOR__' => '5',
            '__VERSION_MINOR__' => '4003',
            '__VERSION_TINY__' => '0',
            '__VERSION_BUILD__' => '0',
            '__BUILD_NUMBER_MAJOR__' => '5',
            '__BUILD_NUMBER_MINOR__' => '4003',
            '__BUILD_NUMBER_VARIANT__' => '0',
            '__FULL_BUILD_MAJOR_VERSION__' => '5',
        },
    },

    {
        'RC_ProjectSourceVersion' => '5300.4',
        expectedResults => {
            '__VERSION_TEXT__' => '300.4',
            '__BUILD_NUMBER__' => '300.4',
            '__BUILD_NUMBER_SHORT__' => '300.4',
            '__VERSION_MAJOR__' => '300',
            '__VERSION_MINOR__' => '4000',
            '__VERSION_TINY__' => '0',
            '__VERSION_BUILD__' => '0',
            '__BUILD_NUMBER_MAJOR__' => '300',
            '__BUILD_NUMBER_MINOR__' => '4000',
            '__BUILD_NUMBER_VARIANT__' => '0',
            '__FULL_BUILD_MAJOR_VERSION__' => '5300',
        },
    },

    {
        'RC_ProjectSourceVersion' => '530.4',
        expectedResults => {
            '__VERSION_TEXT__' => '530.4',
            '__BUILD_NUMBER__' => '530.4',
            '__BUILD_NUMBER_SHORT__' => '530.4',
            '__VERSION_MAJOR__' => '530',
            '__VERSION_MINOR__' => '4000',
            '__VERSION_TINY__' => '0',
            '__VERSION_BUILD__' => '0',
            '__BUILD_NUMBER_MAJOR__' => '530',
            '__BUILD_NUMBER_MINOR__' => '4000',
            '__BUILD_NUMBER_VARIANT__' => '0',
            '__FULL_BUILD_MAJOR_VERSION__' => '530',
        },
    },

    {
        'RC_ProjectSourceVersion' => '53.4',
        expectedResults => {
            '__VERSION_TEXT__' => '53.4',
            '__BUILD_NUMBER__' => '53.4',
            '__BUILD_NUMBER_SHORT__' => '53.4',
            '__VERSION_MAJOR__' => '53',
            '__VERSION_MINOR__' => '4000',
            '__VERSION_TINY__' => '0',
            '__VERSION_BUILD__' => '0',
            '__BUILD_NUMBER_MAJOR__' => '53',
            '__BUILD_NUMBER_MINOR__' => '4000',
            '__BUILD_NUMBER_VARIANT__' => '0',
            '__FULL_BUILD_MAJOR_VERSION__' => '53',
        },
    },

    {
        'RC_ProjectSourceVersion' => '5.4',
        expectedResults => {
            '__VERSION_TEXT__' => '5.4',
            '__BUILD_NUMBER__' => '5.4',
            '__BUILD_NUMBER_SHORT__' => '5.4',
            '__VERSION_MAJOR__' => '5',
            '__VERSION_MINOR__' => '4000',
            '__VERSION_TINY__' => '0',
            '__VERSION_BUILD__' => '0',
            '__BUILD_NUMBER_MAJOR__' => '5',
            '__BUILD_NUMBER_MINOR__' => '4000',
            '__BUILD_NUMBER_VARIANT__' => '0',
            '__FULL_BUILD_MAJOR_VERSION__' => '5',
        },
    },

    {
        'RC_ProjectSourceVersion' => '5300',
        expectedResults => {
            '__VERSION_TEXT__' => '300',
            '__BUILD_NUMBER__' => '300',
            '__BUILD_NUMBER_SHORT__' => '300',
            '__VERSION_MAJOR__' => '300',
            '__VERSION_MINOR__' => '0',
            '__VERSION_TINY__' => '0',
            '__VERSION_BUILD__' => '0',
            '__BUILD_NUMBER_MAJOR__' => '300',
            '__BUILD_NUMBER_MINOR__' => '0',
            '__BUILD_NUMBER_VARIANT__' => '0',
            '__FULL_BUILD_MAJOR_VERSION__' => '5300',
        },
    },

    {
        'RC_ProjectSourceVersion' => '530',
        expectedResults => {
            '__VERSION_TEXT__' => '530',
            '__BUILD_NUMBER__' => '530',
            '__BUILD_NUMBER_SHORT__' => '530',
            '__VERSION_MAJOR__' => '530',
            '__VERSION_MINOR__' => '0',
            '__VERSION_TINY__' => '0',
            '__VERSION_BUILD__' => '0',
            '__BUILD_NUMBER_MAJOR__' => '530',
            '__BUILD_NUMBER_MINOR__' => '0',
            '__BUILD_NUMBER_VARIANT__' => '0',
            '__FULL_BUILD_MAJOR_VERSION__' => '530',
        },
    },

    {
        'RC_ProjectSourceVersion' => '10530.1.1.1',
        expectedResults => {
            '__VERSION_TEXT__' => '530.1.1.1',
            '__BUILD_NUMBER__' => '530.1.1.1',
            '__BUILD_NUMBER_SHORT__' => '530.1.1.1',
            '__VERSION_MAJOR__' => '530',
            '__VERSION_MINOR__' => '1001',
            '__VERSION_TINY__' => '1000',
            '__VERSION_BUILD__' => '1',
            '__BUILD_NUMBER_MAJOR__' => '530',
            '__BUILD_NUMBER_MINOR__' => '1001',
            '__BUILD_NUMBER_VARIANT__' => '1000',
            '__FULL_BUILD_MAJOR_VERSION__' => '10530',
        },
    },

    {
        'RC_ProjectSourceVersion' => '10530.30.20.10',
        expectedResults => {
            '__VERSION_TEXT__' => '530.30.20.10',
            '__BUILD_NUMBER__' => '530.30.20.10',
            '__BUILD_NUMBER_SHORT__' => '530.30.20.10',
            '__VERSION_MAJOR__' => '530',
            '__VERSION_MINOR__' => '30020',
            '__VERSION_TINY__' => '10000',
            '__VERSION_BUILD__' => '10',
            '__BUILD_NUMBER_MAJOR__' => '530',
            '__BUILD_NUMBER_MINOR__' => '30020',
            '__BUILD_NUMBER_VARIANT__' => '10000',
            '__FULL_BUILD_MAJOR_VERSION__' => '10530',
        },
    },

    {
        'RC_ProjectSourceVersion' => '10530.64.200.64',
        expectedResults => {
            '__VERSION_TEXT__' => '530.64.200.64',
            '__BUILD_NUMBER__' => '530.64.200.64',
            '__BUILD_NUMBER_SHORT__' => '530.64.200.64',
            '__VERSION_MAJOR__' => '530',
            '__VERSION_MINOR__' => '64200',
            '__VERSION_TINY__' => '64000',
            '__VERSION_BUILD__' => '64',
            '__BUILD_NUMBER_MAJOR__' => '530',
            '__BUILD_NUMBER_MINOR__' => '64200',
            '__BUILD_NUMBER_VARIANT__' => '64000',
            '__FULL_BUILD_MAJOR_VERSION__' => '10530',
        },
    },

    {
        'RC_ProjectSourceVersion' => '10530.64.999.64',
        expectedResults => {
            '__VERSION_TEXT__' => '530.64.999.64',
            '__BUILD_NUMBER__' => '530.64.999.64',
            '__BUILD_NUMBER_SHORT__' => '530.64.999.64',
            '__VERSION_MAJOR__' => '530',
            '__VERSION_MINOR__' => '64999',
            '__VERSION_TINY__' => '64000',
            '__VERSION_BUILD__' => '64',
            '__BUILD_NUMBER_MAJOR__' => '530',
            '__BUILD_NUMBER_MINOR__' => '64999',
            '__BUILD_NUMBER_VARIANT__' => '64000',
            '__FULL_BUILD_MAJOR_VERSION__' => '10530',
        },
    },

    {
        'RC_ProjectSourceVersion' => '7530.64.99.10',
        expectedResults => {
            '__VERSION_TEXT__' => '530.64.99.10',
            '__BUILD_NUMBER__' => '530.64.99.10',
            '__BUILD_NUMBER_SHORT__' => '530.64.99.10',
            '__VERSION_MAJOR__' => '530',
            '__VERSION_MINOR__' => '64099',
            '__VERSION_TINY__' => '10000',
            '__VERSION_BUILD__' => '10',
            '__BUILD_NUMBER_MAJOR__' => '530',
            '__BUILD_NUMBER_MINOR__' => '64099',
            '__BUILD_NUMBER_VARIANT__' => '10000',
            '__FULL_BUILD_MAJOR_VERSION__' => '7530',
        },
    },

    # Smallest "Valid" case
    {
        'RC_ProjectSourceVersion' => '53',
        expectedResults => {
            '__VERSION_TEXT__' => '53',
            '__BUILD_NUMBER__' => '53',
            '__BUILD_NUMBER_SHORT__' => '53',
            '__VERSION_MAJOR__' => '53',
            '__VERSION_MINOR__' => '0',
            '__VERSION_TINY__' => '0',
            '__VERSION_BUILD__' => '0',
            '__BUILD_NUMBER_MAJOR__' => '53',
            '__BUILD_NUMBER_MINOR__' => '0',
            '__BUILD_NUMBER_VARIANT__' => '0',
            '__FULL_BUILD_MAJOR_VERSION__' => '53',
        },
    },

    # We don't support 1-digit versions, but we should run without crashing
    {
        'RC_ProjectSourceVersion' => '5',
        expectedResults => {
            '__VERSION_TEXT__' => '5',
            '__BUILD_NUMBER__' => '5',
            '__BUILD_NUMBER_SHORT__' => '5',
            '__VERSION_MAJOR__' => '5',
            '__VERSION_MINOR__' => '0',
            '__VERSION_TINY__' => '0',
            '__VERSION_BUILD__' => '0',
            '__BUILD_NUMBER_MAJOR__' => '5',
            '__BUILD_NUMBER_MINOR__' => '0',
            '__BUILD_NUMBER_VARIANT__' => '0',
            '__FULL_BUILD_MAJOR_VERSION__' => '5',
        },
    },

    # Largest specified version test
    {
        'RC_ProjectSourceVersion' => '214747.64.999.64.999',
        expectedResults => {
            '__VERSION_TEXT__' => '747.64.999.64.999',
            '__BUILD_NUMBER__' => '747.64.999.64.999',
            '__BUILD_NUMBER_SHORT__' => '747.64.999.64.999',
            '__VERSION_MAJOR__' => '747',
            '__VERSION_MINOR__' => '64999',
            '__VERSION_TINY__' => '64999',
            '__VERSION_BUILD__' => '999',
            '__BUILD_NUMBER_MAJOR__' => '747',
            '__BUILD_NUMBER_MINOR__' => '64999',
            '__BUILD_NUMBER_VARIANT__' => '64999',
            '__FULL_BUILD_MAJOR_VERSION__' => '214747',
        },
    },

    # Leading Whitespace
    {
        'RC_ProjectSourceVersion' => '        214747.64.99.64.99',
        expectedResults => {
            '__VERSION_TEXT__' => '747.64.99.64.99',
            '__BUILD_NUMBER__' => '747.64.99.64.99',
            '__BUILD_NUMBER_SHORT__' => '747.64.99.64.99',
            '__VERSION_MAJOR__' => '747',
            '__VERSION_MINOR__' => '64099',
            '__VERSION_TINY__' => '64099',
            '__VERSION_BUILD__' => '99',
            '__BUILD_NUMBER_MAJOR__' => '747',
            '__BUILD_NUMBER_MINOR__' => '64099',
            '__BUILD_NUMBER_VARIANT__' => '64099',
            '__FULL_BUILD_MAJOR_VERSION__' => '214747',
        },
    },

    # Trailing Whitespace
    {
        'RC_ProjectSourceVersion' => '214747.64.99.64.99      ',
        expectedResults => {
            '__VERSION_TEXT__' => '747.64.99.64.99',
            '__BUILD_NUMBER__' => '747.64.99.64.99',
            '__BUILD_NUMBER_SHORT__' => '747.64.99.64.99',
            '__VERSION_MAJOR__' => '747',
            '__VERSION_MINOR__' => '64099',
            '__VERSION_TINY__' => '64099',
            '__VERSION_BUILD__' => '99',
            '__BUILD_NUMBER_MAJOR__' => '747',
            '__BUILD_NUMBER_MINOR__' => '64099',
            '__BUILD_NUMBER_VARIANT__' => '64099',
            '__FULL_BUILD_MAJOR_VERSION__' => '214747',
        },
    },

    # Leading and Trailing Whitespace
    {
        'RC_ProjectSourceVersion' => '        214747.64.99.64.99      ',
        expectedResults => {
            '__VERSION_TEXT__' => '747.64.99.64.99',
            '__BUILD_NUMBER__' => '747.64.99.64.99',
            '__BUILD_NUMBER_SHORT__' => '747.64.99.64.99',
            '__VERSION_MAJOR__' => '747',
            '__VERSION_MINOR__' => '64099',
            '__VERSION_TINY__' => '64099',
            '__VERSION_BUILD__' => '99',
            '__BUILD_NUMBER_MAJOR__' => '747',
            '__BUILD_NUMBER_MINOR__' => '64099',
            '__BUILD_NUMBER_VARIANT__' => '64099',
            '__FULL_BUILD_MAJOR_VERSION__' => '214747',
        },
    },

    # Use RC_PROJECTBUILDVERSION or RC_ProjectBuildVersion if our submission version
    # does not have 4 or more tuples.
    {
        'RC_ProjectSourceVersion' => '7600.1',
        'RC_ProjectBuildVersion' => '26',
        expectedResults => {
            '__VERSION_TEXT__' => '600.1',
            '__BUILD_NUMBER__' => '600.1',
            '__BUILD_NUMBER_SHORT__' => '600.1',
            '__VERSION_MAJOR__' => '600',
            '__VERSION_MINOR__' => '1000',
            '__VERSION_TINY__' => '0',
            '__VERSION_BUILD__' => '26',
            '__BUILD_NUMBER_MAJOR__' => '600',
            '__BUILD_NUMBER_MINOR__' => '1000',
            '__BUILD_NUMBER_VARIANT__' => '0',
            '__FULL_BUILD_MAJOR_VERSION__' => '7600',
        },
    },

    {
        'RC_ProjectSourceVersion' => '27',
        'RC_ProjectBuildVersion' => '14',
        expectedResults => {
            '__VERSION_TEXT__' => '27',
            '__BUILD_NUMBER__' => '27',
            '__BUILD_NUMBER_SHORT__' => '27',
            '__VERSION_MAJOR__' => '27',
            '__VERSION_MINOR__' => '0',
            '__VERSION_TINY__' => '0',
            '__VERSION_BUILD__' => '14',
            '__BUILD_NUMBER_MAJOR__' => '27',
            '__BUILD_NUMBER_MINOR__' => '0',
            '__BUILD_NUMBER_VARIANT__' => '0',
            '__FULL_BUILD_MAJOR_VERSION__' => '27',
        },
    },

    {
        'RC_ProjectSourceVersion' => '27',
        'RC_PROJECTBUILDVERSION' => '14',
        expectedResults => {
            '__VERSION_TEXT__' => '27',
            '__BUILD_NUMBER__' => '27',
            '__BUILD_NUMBER_SHORT__' => '27',
            '__VERSION_MAJOR__' => '27',
            '__VERSION_MINOR__' => '0',
            '__VERSION_TINY__' => '0',
            '__VERSION_BUILD__' => '14',
            '__BUILD_NUMBER_MAJOR__' => '27',
            '__BUILD_NUMBER_MINOR__' => '0',
            '__BUILD_NUMBER_VARIANT__' => '0',
            '__FULL_BUILD_MAJOR_VERSION__' => '27',
        },
    },

    {
        'RC_PROJECTSOURCEVERSION' => '27',
        'RC_ProjectBuildVersion' => '14',
        expectedResults => {
            '__VERSION_TEXT__' => '27',
            '__BUILD_NUMBER__' => '27',
            '__BUILD_NUMBER_SHORT__' => '27',
            '__VERSION_MAJOR__' => '27',
            '__VERSION_MINOR__' => '0',
            '__VERSION_TINY__' => '0',
            '__VERSION_BUILD__' => '14',
            '__BUILD_NUMBER_MAJOR__' => '27',
            '__BUILD_NUMBER_MINOR__' => '0',
            '__BUILD_NUMBER_VARIANT__' => '0',
            '__FULL_BUILD_MAJOR_VERSION__' => '27',
        },
    },

    {
        'RC_ProjectSourceVersion' => '5300.4.3.2.1',
        'RC_ProjectBuildVersion' => '14',
        expectedResults => {
            '__VERSION_TEXT__' => '300.4.3.2.1',
            '__BUILD_NUMBER__' => '300.4.3.2.1',
            '__BUILD_NUMBER_SHORT__' => '300.4.3.2.1',
            '__VERSION_MAJOR__' => '300',
            '__VERSION_MINOR__' => '4003',
            '__VERSION_TINY__' => '2001',
            '__VERSION_BUILD__' => '1',
            '__BUILD_NUMBER_MAJOR__' => '300',
            '__BUILD_NUMBER_MINOR__' => '4003',
            '__BUILD_NUMBER_VARIANT__' => '2001',
            '__FULL_BUILD_MAJOR_VERSION__' => '5300',
        },
    },

    {
        'RC_ProjectSourceVersion' => '5300.4.3.2.1',
        'RC_PROJECTBUILDVERSION' => '14',
        expectedResults => {
            '__VERSION_TEXT__' => '300.4.3.2.1',
            '__BUILD_NUMBER__' => '300.4.3.2.1',
            '__BUILD_NUMBER_SHORT__' => '300.4.3.2.1',
            '__VERSION_MAJOR__' => '300',
            '__VERSION_MINOR__' => '4003',
            '__VERSION_TINY__' => '2001',
            '__VERSION_BUILD__' => '1',
            '__BUILD_NUMBER_MAJOR__' => '300',
            '__BUILD_NUMBER_MINOR__' => '4003',
            '__BUILD_NUMBER_VARIANT__' => '2001',
            '__FULL_BUILD_MAJOR_VERSION__' => '5300',
        },
    },

    {
        'RC_PROJECTSOURCEVERSION' => '5300.4.3.2.1',
        'RC_ProjectBuildVersion' => '14',
        expectedResults => {
            '__VERSION_TEXT__' => '300.4.3.2.1',
            '__BUILD_NUMBER__' => '300.4.3.2.1',
            '__BUILD_NUMBER_SHORT__' => '300.4.3.2.1',
            '__VERSION_MAJOR__' => '300',
            '__VERSION_MINOR__' => '4003',
            '__VERSION_TINY__' => '2001',
            '__VERSION_BUILD__' => '1',
            '__BUILD_NUMBER_MAJOR__' => '300',
            '__BUILD_NUMBER_MINOR__' => '4003',
            '__BUILD_NUMBER_VARIANT__' => '2001',
            '__FULL_BUILD_MAJOR_VERSION__' => '5300',
        },
    },

    {
        'RC_PROJECTSOURCEVERSION' => '5300.4.3.2.1',
        'RC_PROJECTBUILDVERSION' => '14',
        expectedResults => {
            '__VERSION_TEXT__' => '300.4.3.2.1',
            '__BUILD_NUMBER__' => '300.4.3.2.1',
            '__BUILD_NUMBER_SHORT__' => '300.4.3.2.1',
            '__VERSION_MAJOR__' => '300',
            '__VERSION_MINOR__' => '4003',
            '__VERSION_TINY__' => '2001',
            '__VERSION_BUILD__' => '1',
            '__BUILD_NUMBER_MAJOR__' => '300',
            '__BUILD_NUMBER_MINOR__' => '4003',
            '__BUILD_NUMBER_VARIANT__' => '2001',
            '__FULL_BUILD_MAJOR_VERSION__' => '5300',
        },
    },
);

# This test should only be run on Windows
if ($^O ne 'MSWin32' && $^O ne 'cygwin') {
    plan(tests => 1);
    is(1, 1, 'do nothing for non-Windows builds.');
    exit 0;    
}

my $testCasesCount = scalar(@testCases) * 11; # 11 expected results
plan(tests => $testCasesCount);

foreach my $testCase (@testCases) {
    my $toolsPath = $ENV{'WEBKIT_LIBRARIES'};
    my $autoVersionScript = File::Spec->catfile($toolsPath, 'tools', 'scripts', 'auto-version.pl');
    my $testOutputDir = tempdir(CLEANUP => 1);
    my $testFlags;
    if ($testCase->{'RC_ProjectSourceVersion'}) {
        $testFlags = " RC_ProjectSourceVersion=\"$testCase->{'RC_ProjectSourceVersion'}\"";
    } elsif ($testCase->{'RC_PROJECTSOURCEVERSION'}) {
        $testFlags = " RC_PROJECTSOURCEVERSION=\"$testCase->{'RC_PROJECTSOURCEVERSION'}\"";
    } else {
        die ("Missing the RC_ProjectSourceVersion environment variable.\n");
    }

    if ($testCase->{'RC_ProjectBuildVersion'}) {
        $testFlags .= " RC_ProjectBuildVersion=\"$testCase->{'RC_ProjectBuildVersion'}\"";
    } elsif ($testCase->{'RC_PROJECTBUILDVERSION'}) {
        $testFlags .= " RC_PROJECTBUILDVERSION=\"$testCase->{'RC_PROJECTBUILDVERSION'}\"";
    }

    `$testFlags perl $autoVersionScript $testOutputDir`;

    my $expectedResults = $testCase->{expectedResults};

    my $outputFile = File::Spec->catfile($testOutputDir, 'include', 'autoversion.h');
    open(TEST_OUTPUT, '<', $outputFile) or die "Unable to open $outputFile: $!";

    while (my $line = <TEST_OUTPUT>) {
        foreach my $expectedResultKey (keys %$expectedResults) {
            if ($line !~ m/$expectedResultKey/) {
                next;
            }

            $line =~ s/#define $expectedResultKey//;
            $line =~ s/^\s*(.*)\s*$/$1/;
            $line =~ s/^"(.*)"$/$1/;
            chomp($line);

            my $expectedResultValue = $expectedResults->{$expectedResultKey};
            if ($testCase->{'RC_ProjectSourceVersion'}) {
                is($line, $expectedResultValue, "$testCase->{'RC_ProjectSourceVersion'}: $expectedResultKey");
            } else {
                is($line, $expectedResultValue, "$testCase->{'RC_PROJECTSOURCEVERSION'}: $expectedResultKey");
            }
        }
    }

    close(TEST_OUTPUT);
}
