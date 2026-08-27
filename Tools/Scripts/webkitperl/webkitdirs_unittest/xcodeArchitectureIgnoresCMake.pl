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

# Unit test for webkitdirs::xcodeArchitecture. Both functions cache their answer
# in a script-global, so the --architecture scenario lives in
# xcodeArchitectureUserSpecified.pl (test-webkitperl runs each in a fresh process).
use strict;
use warnings;
use File::Spec;
use File::Temp qw(tempdir);
use Test::More;
use webkitdirs;

plan(tests => 2);

# webkitperl runs on non-Cocoa bots too, so fake a Cocoa port with a cmake-mac tree.
no warnings qw(redefine prototype);
*webkitdirs::isAppleCocoaWebKit = sub () { 1 };
*webkitdirs::isCMakeBuild = sub () { 1 };
*webkitdirs::defaultArchitectureForXcodeSDK = sub { "arm64e" };
# determineArchitecture()'s cross-compilation branch reads $CC instead of shelling out to `cmake --system-information`.
*webkitdirs::isCrossCompilation = sub () { 1 };
use warnings qw(redefine prototype);

my $stubCompiler = File::Spec->catfile(tempdir(CLEANUP => 1), "cc");
open my $fh, ">", $stubCompiler or die "Can't write stub compiler: $!";
print $fh "#!/bin/sh\necho arm64-apple-darwin\n";
close $fh;
chmod 0755, $stubCompiler or die "Can't make stub compiler executable: $!";
$ENV{'CC'} = $stubCompiler;

@ARGV = ();

is(architecture(), "arm64", "architecture() reports what the CMake build targets");
is(webkitdirs::xcodeArchitecture(), "arm64e", "xcodeArchitecture() reports the SDK's default slice");
