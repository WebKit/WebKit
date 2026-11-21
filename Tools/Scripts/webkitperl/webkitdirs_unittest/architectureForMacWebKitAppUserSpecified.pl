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


# Separate .pl file for this `--architecture` scenario since architecture()
# maintains a script-global cache.
use strict;
use warnings;
use File::Temp qw(tempdir);
use Test::More;
use webkitdirs;

plan(tests => 2);

no warnings qw(redefine prototype);
*webkitdirs::machOArchitectures = sub ($) { ("arm64") };
use warnings qw(redefine prototype);

setBaseProductDir(tempdir(CLEANUP => 1));

@ARGV = ("--architecture", "x86_64", "leftover");
my $appPath = "/Build/Release/MiniBrowser.app/Contents/MacOS/MiniBrowser";
is(webkitdirs::architectureForMacWebKitApp($appPath, "/Build/Release"), "x86_64", "--architecture is used as given");
is_deeply(\@ARGV, ["leftover"], "--architecture is removed from \@ARGV");
