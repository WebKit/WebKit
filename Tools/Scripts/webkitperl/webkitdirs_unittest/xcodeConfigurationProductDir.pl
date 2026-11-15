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

# Unit test for webkitdirs::xcodeConfigurationProductDir sanitizer suffixes.
use strict;
use warnings;
use File::Spec;
use File::Temp qw(tempdir);
use Test::More;
use webkitdirs;

plan(tests => 10);

my $asanIsEnabled = 0;
my $tsanIsEnabled = 0;
my $isEmbedded = 0;
my $isMacCatalyst = 0;
my $platformName = "macosx";
my $usesPerConfigDir = 0;

no warnings qw(redefine prototype);
*webkitdirs::isAppleCocoaWebKit = sub () { 1 };
*webkitdirs::isCMakeBuild = sub () { 0 };
*webkitdirs::asanIsEnabled = sub () { return $asanIsEnabled; };
*webkitdirs::tsanIsEnabled = sub () { return $tsanIsEnabled; };
*webkitdirs::isEmbeddedWebKit = sub () { return $isEmbedded; };
*webkitdirs::isMacCatalystWebKit = sub () { return $isMacCatalyst; };
*webkitdirs::xcodeSDKPlatformName = sub () { return $platformName; };
*webkitdirs::usesPerConfigurationBuildDirectory = sub () { return $usesPerConfigDir; };
use warnings qw(redefine prototype);

my $base = tempdir(CLEANUP => 1);
setBaseProductDir($base);

setConfiguration("Release");
is(webkitdirs::xcodeConfigurationProductDir(), File::Spec->catdir($base, "Release"), "macOS Release without sanitizers");

$asanIsEnabled = 1;
is(webkitdirs::xcodeConfigurationProductDir(), File::Spec->catdir($base, "Release-asan"), "macOS Release with ASan");

setConfiguration("Debug");
is(webkitdirs::xcodeConfigurationProductDir(), File::Spec->catdir($base, "Debug-asan"), "macOS Debug with ASan");

$asanIsEnabled = 0;
$tsanIsEnabled = 1;
is(webkitdirs::xcodeConfigurationProductDir(), File::Spec->catdir($base, "Debug-tsan"), "macOS Debug with TSan");

$asanIsEnabled = 1;
$tsanIsEnabled = 1;
is(webkitdirs::xcodeConfigurationProductDir(), File::Spec->catdir($base, "Debug-asan"), "ASan takes precedence over TSan");

$tsanIsEnabled = 0;
$isEmbedded = 1;
$platformName = "iphoneos";
setConfiguration("Release");
is(webkitdirs::xcodeConfigurationProductDir(), File::Spec->catdir($base, "Release-iphoneos-asan"), "embedded Release with ASan");

$asanIsEnabled = 0;
is(webkitdirs::xcodeConfigurationProductDir(), File::Spec->catdir($base, "Release-iphoneos"), "embedded Release without sanitizers");

$isEmbedded = 0;
$isMacCatalyst = 1;
$platformName = "maccatalyst";
$asanIsEnabled = 1;
is(webkitdirs::xcodeConfigurationProductDir(), File::Spec->catdir($base, "Release-maccatalyst-asan"), "Mac Catalyst Release with ASan");

$isMacCatalyst = 0;
$platformName = "macosx";
setConfiguration("Production");
is(webkitdirs::xcodeConfigurationProductDir(), File::Spec->catdir($base, "Production"), "Production ignores sanitizer suffix");

setConfiguration("Release");
$usesPerConfigDir = 1;
is(webkitdirs::xcodeConfigurationProductDir(), $base, "WEBKIT_OUTPUTDIR ignores sanitizer suffix");
