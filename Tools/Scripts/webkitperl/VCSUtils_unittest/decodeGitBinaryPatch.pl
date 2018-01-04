#!/usr/bin/env perl
#
# Copyright (C) 2015 Apple Inc.  All rights reserved.
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

# Unit tests for decodeGitBinaryPatch()

use strict;
use warnings;

use Test::More;
use VCSUtils;

my @testCaseHashRefs = (
{
    diffName => "Single line chunks",
    inputText => <<'END',
index 490e319f550754ff97dbc4d5a69ad7dbc69c85da..7764e296b482c0ff2c9e4f04114cee9fa1f7544f 100644
GIT binary patch
delta 12
Tcmdnzb;WhVcGk^`9I~<iCFlgC

delta 12
Tcmdnzb;WhVcGk^`9I~<iCFlgC

END
    fullPath => "foo/bar",
    expectedReturn => [
        'delta',
        VCSUtils::decodeGitBinaryChunk('Tcmdnzb;WhVcGk^`9I~<iCFlgC', "foo/bar"),
        'delta',
        VCSUtils::decodeGitBinaryChunk('Tcmdnzb;WhVcGk^`9I~<iCFlgC', "foo/bar"),
    ],
},
{
    diffName => "Multiline line chunks",
    inputText => <<'END',
index 490e319f550754ff97dbc4d5a69ad7dbc69c85da..7764e296b482c0ff2c9e4f04114cee9fa1f7544f 100644
GIT binary patch
delta 12
Tcmdnzb;WhVcGk^`9I~<iCFlgC

delta 12
zuDr5uCY6G+qE&tSt8Hf1`e-KmRHU1=EbP;&Vn8(El#%J`5jjQa<69Qy^WA8twJa=e
zKA=23zh&X#=9RtE2ev9KEAFrPu53C~W&er+>0h@h?9_e$t9tRYnNwz6%qmv)>r07~
z(npNQ>6pss-hG-BQ`xUKqyDnCRhen7(g;<(n-~KU(~z0dtcE;ZqD|&MTInj%e{n1M
z%E0#7lQ5vNzoxn>y*jsW?-djFDBQQ*K3#hs+NZMgm!)^#@~ZxQ`d0Mso6dZV^n)D=
zZz*ZUS2-6H?wvlpqOj}k?}x6)d~fuM+baq`D*FGwuWD4AqtpM|m2=PUW{)i7Uwi%)

END
    fullPath => "foo/bar",
    expectedReturn => [
        'delta',
        VCSUtils::decodeGitBinaryChunk('Tcmdnzb;WhVcGk^`9I~<iCFlgC', "foo/bar"),
        'delta',
        VCSUtils::decodeGitBinaryChunk('zuDr5uCY6G+qE&tSt8Hf1`e-KmRHU1=EbP;&Vn8(El#%J`5jjQa<69Qy^WA8twJa=e
        zKA=23zh&X#=9RtE2ev9KEAFrPu53C~W&er+>0h@h?9_e$t9tRYnNwz6%qmv)>r07~
        z(npNQ>6pss-hG-BQ`xUKqyDnCRhen7(g;<(n-~KU(~z0dtcE;ZqD|&MTInj%e{n1M
        z%E0#7lQ5vNzoxn>y*jsW?-djFDBQQ*K3#hs+NZMgm!)^#@~ZxQ`d0Mso6dZV^n)D=
        zZz*ZUS2-6H?wvlpqOj}k?}x6)d~fuM+baq`D*FGwuWD4AqtpM|m2=PUW{)i7Uwi%)', "foo/bar"),
    ],
},
{
    diffName => "Multiline line chunks and as last patch",
    inputText => <<'END',
index 490e319f550754ff97dbc4d5a69ad7dbc69c85da..7764e296b482c0ff2c9e4f04114cee9fa1f7544f 100644
GIT binary patch
delta 12
Tcmdnzb;WhVcGk^`9I~<iCFlgC

delta 12
zuDr5uCY6G+qE&tSt8Hf1`e-KmRHU1=EbP;&Vn8(El#%J`5jjQa<69Qy^WA8twJa=e
zKA=23zh&X#=9RtE2ev9KEAFrPu53C~W&er+>0h@h?9_e$t9tRYnNwz6%qmv)>r07~
z(npNQ>6pss-hG-BQ`xUKqyDnCRhen7(g;<(n-~KU(~z0dtcE;ZqD|&MTInj%e{n1M
z%E0#7lQ5vNzoxn>y*jsW?-djFDBQQ*K3#hs+NZMgm!)^#@~ZxQ`d0Mso6dZV^n)D=
zZz*ZUS2-6H?wvlpqOj}k?}x6)d~fuM+baq`D*FGwuWD4AqtpM|m2=PUW{)i7Uwi%)

-- 
1.7.4.4
END
    fullPath => "foo/bar",
    expectedReturn => [
        'delta',
        VCSUtils::decodeGitBinaryChunk('Tcmdnzb;WhVcGk^`9I~<iCFlgC', "foo/bar"),
        'delta',
        VCSUtils::decodeGitBinaryChunk('zuDr5uCY6G+qE&tSt8Hf1`e-KmRHU1=EbP;&Vn8(El#%J`5jjQa<69Qy^WA8twJa=e
        zKA=23zh&X#=9RtE2ev9KEAFrPu53C~W&er+>0h@h?9_e$t9tRYnNwz6%qmv)>r07~
        z(npNQ>6pss-hG-BQ`xUKqyDnCRhen7(g;<(n-~KU(~z0dtcE;ZqD|&MTInj%e{n1M
        z%E0#7lQ5vNzoxn>y*jsW?-djFDBQQ*K3#hs+NZMgm!)^#@~ZxQ`d0Mso6dZV^n)D=
        zZz*ZUS2-6H?wvlpqOj}k?}x6)d~fuM+baq`D*FGwuWD4AqtpM|m2=PUW{)i7Uwi%)', "foo/bar"),
    ],
}
);

my $testCasesCount = @testCaseHashRefs;
plan(tests => $testCasesCount); # Total number of assertions.

foreach my $testCase (@testCaseHashRefs) {
    my $testNameStart = "decodeGitBinaryPatch(): $testCase->{diffName}: Comparing";


    my @got = VCSUtils::decodeGitBinaryPatch($testCase->{inputText}, $testCase->{fullPath});
    my $expectedReturn = $testCase->{expectedReturn};

    is_deeply(\@got, $expectedReturn, "$testNameStart return value.");

}
