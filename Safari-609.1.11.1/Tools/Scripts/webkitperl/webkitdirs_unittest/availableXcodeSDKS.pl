#!/usr/bin/env perl

# Copyright (C) 2018 Apple Inc. All rights reserved.
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

# Unit tests for webkitdirs::parseAvailableXcodeSDKs($).

use strict;
use warnings;

use Config;
use Test::More;
use webkitdirs;

plan(tests => 2);

my @fullXcodebuildOutput = <<END =~ m/(^.*\n)/mg;
iOS SDKs:
iOS 12.0                          -sdk iphoneos12.0
iOS 12.0 Internal                 -sdk iphoneos12.0.internal

iOS Simulator SDKs:
Simulator - iOS 12.0 Internal     -sdk iphonesimulator12.0

macOS SDKs:
macOS 10.14                       -sdk macosx10.14
macOS 10.14 Internal              -sdk macosx10.14internal

END

my @result = parseAvailableXcodeSDKs(\@fullXcodebuildOutput);
my @expectedResult = ("iphoneos", "iphoneos.internal", "iphonesimulator", "macosx", "macosx.internal");
is_deeply(\@result, \@expectedResult, "parseAvailableXcodeSDKs: Full xcodebuild output");

my @closeMatchOutput = <<END =~ m/(^.*\n)/mg;
Non-matching SDKs:
watchOS 5.0                       -SDK watchos5.0
tvOS 12.0                         -sdk appletvos
iOS 12.0                          -SDK iphoneos12.0.internal.4

END

my @emptyList = ();
@result = parseAvailableXcodeSDKs(\@closeMatchOutput);
is_deeply(\@result, \@emptyList, "parseAvailableXcodeSDKs: Near matches");
