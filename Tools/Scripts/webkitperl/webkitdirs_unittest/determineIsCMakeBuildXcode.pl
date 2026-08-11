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

# Unit test for webkitdirs::determineIsCMakeBuild --xcode handling.
# The function caches its answer in a script-global, so each scenario
# lives in its own .pl file (test-webkitperl runs each in a fresh process).
use strict;
use warnings;
use Test::More;
use webkitdirs;

plan(tests => 2);

# isCMakeBuild() short-circuits to true on non-Cocoa ports, so force
# isAppleCocoaWebKit() on to keep the --xcode assertion platform-independent
# (webkitperl runs on non-Cocoa bots too).
no warnings qw(redefine prototype);
*webkitdirs::isAppleCocoaWebKit = sub () { 1 };
use warnings qw(redefine prototype);

@ARGV = ("--xcode", "leftover");
webkitdirs::determineIsCMakeBuild();

is_deeply(\@ARGV, ["leftover"], "--xcode is removed from \@ARGV");
ok(!isCMakeBuild(), "--xcode pins isCMakeBuild() to false");
