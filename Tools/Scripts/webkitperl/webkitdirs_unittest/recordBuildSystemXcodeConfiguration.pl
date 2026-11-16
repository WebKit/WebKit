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

# Unit test for webkitdirs::recordBuildSystemXcodeConfiguration.
use strict;
use warnings;
use File::Spec;
use File::Temp qw(tempdir);
use Test::More;
use webkitdirs;

plan(tests => 8);

# The build system and the sanitizers are cached in script-globals, so mock the
# accessors rather than the marker files.
my $isCMakeBuild = 1;
my $asanIsEnabled = 0;
no warnings qw(redefine prototype);
*webkitdirs::isAppleCocoaWebKit = sub () { 1 };
*webkitdirs::isCMakeBuild = sub () { return $isCMakeBuild; };
*webkitdirs::asanIsEnabled = sub () { return $asanIsEnabled; };
*webkitdirs::tsanIsEnabled = sub () { return 0; };
use warnings qw(redefine prototype);

my $base = tempdir(CLEANUP => 1);
setBaseProductDir($base);
my $filePath = File::Spec->catfile($base, "BuildSystem.xcconfig");

sub configurationContents()
{
    open(my $fh, "<", $filePath) or return "";
    local $/;
    my $contents = <$fh>;
    close($fh);
    return $contents;
}

sub publishedConfiguration()
{
    return configurationContents() =~ /^WK_CMAKE_CONFIGURATION = (.*);$/m ? $1 : undef;
}

sub modificationTime()
{
    return (stat($filePath))[9];
}

setConfiguration("Release");
recordBuildSystemXcodeConfiguration();

ok(-e $filePath, "a CMake build publishes the xcconfig");
like(configurationContents(), qr/^WK_CMAKE_BASE_PRODUCT_DIR = \Q$base\E;$/m, "it points at the base product directory");
is(publishedConfiguration(), "Release", "a release build names the release tree");

# Date the file in the past rather than compare against the clock, whose
# resolution is coarser than this test runs at.
my $past = time() - 3600;
utime $past, $past, $filePath;
recordBuildSystemXcodeConfiguration();

is(modificationTime(), $past, "an unchanged configuration leaves the xcconfig alone");

setConfiguration("Debug");
recordBuildSystemXcodeConfiguration();

is(publishedConfiguration(), "Debug", "a debug build names the debug tree");
isnt(modificationTime(), $past, "a changed configuration rewrites the xcconfig");

$asanIsEnabled = 1;
recordBuildSystemXcodeConfiguration();

is(publishedConfiguration(), "ASan", "an ASan build names the ASan tree, whatever the configuration");

$isCMakeBuild = 0;
recordBuildSystemXcodeConfiguration();

ok(!-e $filePath, "an Xcode build removes the xcconfig");
