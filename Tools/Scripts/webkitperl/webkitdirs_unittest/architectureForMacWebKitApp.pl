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


# lipo and the default architecture are stubbed so the tests run on any platform.
use strict;
use warnings;
use Test::More;
use webkitdirs;

plan(tests => 14);

my $defaultArchitecture;
my %slices;

no warnings qw(redefine prototype);
*webkitdirs::architecture = sub () { return $defaultArchitecture; };
*webkitdirs::machOArchitectures = sub ($) { @{$slices{$_[0]} // []} };
use warnings qw(redefine prototype);

my $buildDirectory = "/Build/Release";
my $builtWebKit = "$buildDirectory/WebKit.framework/WebKit";
my $miniBrowser = "$buildDirectory/MiniBrowser.app/Contents/MacOS/MiniBrowser";
my $safariForWebKitDevelopment = "/Applications/Safari.app/Contents/MacOS/SafariForWebKitDevelopment";
my $safari = "/Applications/Safari.app/Contents/MacOS/Safari";
my $thirdPartyApp = "/Applications/ThirdParty.app/Contents/MacOS/ThirdParty";
my $intelApp = "/Applications/Intel.app/Contents/MacOS/Intel";

$slices{$safariForWebKitDevelopment} = ["x86_64", "arm64", "arm64e.x1"];
$slices{$safari} = ["x86_64", "arm64e", "arm64e.x1"];
$slices{$thirdPartyApp} = ["x86_64", "arm64"];
$slices{$intelApp} = ["x86_64"];

sub launchArchitecture($)
{
    my ($appPath) = @_;
    return webkitdirs::architectureForMacWebKitApp($appPath, $buildDirectory);
}

# An internal SDK is installed, so the default is arm64e, but WebKit was built against the public SDK.
$defaultArchitecture = "arm64e";
$slices{$builtWebKit} = ["arm64"];
$slices{$miniBrowser} = ["arm64"];
is(launchArchitecture($miniBrowser), "arm64", "a public SDK build launches arm64 even when the default is arm64e");
is(launchArchitecture($safariForWebKitDevelopment), "arm64", "a public SDK build launches SafariForWebKitDevelopment's arm64 slice");

my $launched = eval { launchArchitecture($safari); 1 };
ok(!$launched, "an app with no slice the built WebKit has is not launched");
like($@, qr/^No slice of Safari \(x86_64 arm64e arm64e\.x1\) can load the built WebKit \(arm64\)\./, "the error names both sets of slices");

# An internal SDK build.
$slices{$builtWebKit} = ["arm64e"];
$slices{$miniBrowser} = ["arm64e"];
is(launchArchitecture($miniBrowser), "arm64e", "an internal SDK build launches arm64e");
is(launchArchitecture($safari), "arm64e", "an internal SDK build launches Safari's arm64e slice");
is(launchArchitecture($safariForWebKitDevelopment), "arm64", "an arm64e build runs in SafariForWebKitDevelopment's arm64 slice");
is(launchArchitecture($thirdPartyApp), "arm64", "an arm64e build runs in a third-party app's arm64 slice");
$launched = eval { launchArchitecture($intelApp); 1 };
ok(!$launched, "an app with only an x86_64 slice can't load an arm64e build");

# The default architecture wins when both have it, then arm64e, arm64, and x86_64, in that order.
$slices{$builtWebKit} = ["x86_64", "arm64"];
$slices{$miniBrowser} = ["x86_64", "arm64"];
is(launchArchitecture($miniBrowser), "arm64", "arm64 is preferred to x86_64 when the default is in neither");
$defaultArchitecture = "x86_64";
is(launchArchitecture($miniBrowser), "x86_64", "the default architecture is preferred when both have it");

# Without slices to compare, the default architecture is used as before.
$defaultArchitecture = "arm64e";
delete $slices{$miniBrowser};
is(launchArchitecture($miniBrowser), "arm64e", "an app lipo can't read launches with the default architecture");
$slices{$miniBrowser} = ["arm64"];
delete $slices{$builtWebKit};
is(launchArchitecture($miniBrowser), "arm64e", "without a built WebKit to read, the default architecture is used");

# The first directory with a WebKit.framework is the one that gets loaded.
$slices{"/Build/Safari/WebKit.framework/WebKit"} = ["arm64e"];
$slices{$builtWebKit} = ["arm64"];
is(webkitdirs::architectureForMacWebKitApp($miniBrowser, "/Build/Empty", $buildDirectory, "/Build/Safari"), "arm64", "the first framework directory with a WebKit.framework decides");
