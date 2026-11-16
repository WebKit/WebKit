#!/usr/bin/env perl
#
# Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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

# NOTE: A Bugzilla Comment XML looks like:
#
#     <long_desc isprivate="0">
#       <commentid>1153467</commentid>
#       <who name="Radar WebKit Bug Importer">webkit-bug-importer</who>
#       <bug_when>2016-01-07 11:06:14 -0800</bug_when>
#       <thetext>&lt;rdar://problem/24093563&gt;</thetext>
#     </long_desc>
#
# But for these tests we simplify the XML to just the <thetext> blocks.

my @testCaseHashRefs = (

###
#  Comments where a Radar URL will be detected.
##
{
    testName => "Radar URL comment",
    inputText => "<thetext>&lt;rdar://problem/24093563&gt;</thetext>",
    expected => "<rdar://problem/24093563>"
},
{
    testName => "Radar URL comment no problem path",
    inputText => "<thetext>&lt;rdar://24093563&gt;</thetext>",
    expected => "<rdar://24093563>"
},
{
    testName => "Radar URL comment no angle brackes",
    inputText => "<thetext>rdar://problem/24093563</thetext>",
    expected => "rdar://problem/24093563"
},
{
    testName => "Radar URL comment no angle brackes no problem path",
    inputText => "<thetext>rdar://24093563</thetext>",
    expected => "rdar://24093563"
},
{
    testName => "Radar URL comment with leading whitespace",
    inputText => "<thetext>   &lt;rdar://problem/24093563&gt;</thetext>",
    expected => "<rdar://problem/24093563>"
},
{
    testName => "Radar URL comment with leading whitespace no angle brackets",
    inputText => "<thetext>   rdar://problem/24093563</thetext>",
    expected => "rdar://problem/24093563"
},
{
    testName => "Radar URL comment with leading whitespace no problem path",
    inputText => "<thetext>   &lt;rdar://24093563&gt;</thetext>",
    expected => "<rdar://24093563>"
},
{
    testName => "Radar URL comment with leading whitespace no problem path no angle brackets",
    inputText => "<thetext>   rdar://24093563</thetext>",
    expected => "rdar://24093563"
},
{
    testName => "Radar URL comment with trailing title",
    inputText => "<thetext>&lt;rdar://problem/24093563&gt; Radar Title Here</thetext>",
    expected => "<rdar://problem/24093563>"
},
{
    testName => "Radar URL comment with trailing title no problem path",
    inputText => "<thetext>&lt;rdar://24093563&gt; Radar Title Here</thetext>",
    expected => "<rdar://24093563>"
},
{
    testName => "Radar URL comment with trailing title no angle brackets",
    inputText => "<thetext>rdar://problem/24093563 Radar Title Here</thetext>",
    expected => "rdar://problem/24093563"
},
{
    testName => "Radar URL comment with trailing title no angle brackets no problem path",
    inputText => "<thetext>rdar://24093563 (Radar Title Here)</thetext>",
    expected => "rdar://24093563"
},
{
    testName => "Multiple comments, detect first Radar URL comment",
    inputText => "<thetext>Comment 1</thetext>\n<thetext>&lt;rdar://problem/24093563&gt;</thetext>\n<thetext>&lt;rdar://problem/99999999&gt;</thetext>",
    expected => "<rdar://problem/24093563>"
},
{
    testName => "Multiple comments, detect first Radar URL comment no problem path",
    inputText => "<thetext>Comment 1</thetext>\n<thetext>&lt;rdar://24093563&gt;</thetext>\n<thetext>&lt;rdar://99999999&gt;</thetext>",
    expected => "<rdar://24093563>"
},
{
    testName => "Multiple comments, detect first Radar URL comment no angle brackets",
    inputText => "<thetext>Comment 1</thetext>\n<thetext>rdar://problem/24093563</thetext>\n<thetext>rdar://problem/99999999</thetext>",
    expected => "rdar://problem/24093563"
},
{
    testName => "Multiple comments, detect first Radar URL comment no angle brackets no problem path",
    inputText => "<thetext>Comment 1</thetext>\n<thetext>rdar://24093563</thetext>\n<thetext>rdar://99999999</thetext>",
    expected => "rdar://24093563"
},

###
#  Comments where a Radar URL will not be detected.
##
{
    testName => "Empty comment",
    inputText => "",
    expected => ""
},
{
    testName => "Comment without radar URL link",
    inputText => "<thetext>Comment text</thetext>",
    expected => ""
},
{
    testName => "Radar URL comment with leading text",
    inputText => "<thetext>Check out: &lt;rdar://problem/24093563&gt;</thetext>",
    expected => ""
},

);

# Ignore STDERR output from fetchRadarURLFromBugXMLData for the test cases.
close STDERR;

my $testCasesCount = @testCaseHashRefs;
plan(tests => $testCasesCount);

foreach my $testCase (@testCaseHashRefs) {
    my $expected = $testCase->{expected};
    my $got = PrepareChangeLog::fetchRadarURLFromBugXMLData(152839, $testCase->{inputText});
    is($got, $expected, $testCase->{testName});
}
