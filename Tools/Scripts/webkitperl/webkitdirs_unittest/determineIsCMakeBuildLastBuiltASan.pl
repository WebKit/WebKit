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

# Unit test for webkitdirs::determineIsCMakeBuild last-built tiebreaker with a
# sanitizer build: a CMake ASan build lands in cmake-mac/ASan, so when ASan is
# enabled the tiebreaker must inspect that tree. Each scenario lives in its own
# .pl file because determineIsCMakeBuild caches its answer in a script-global.
use strict;
use warnings;
use File::Path qw(make_path);
use File::Spec;
use File::Temp qw(tempdir);
use Test::More;
use webkitdirs;

plan(tests => 1);

# The tiebreaker is gated on isAppleCocoaWebKit(); force it on so the test
# is platform-independent (webkitperl runs on non-Cocoa bots too).
no warnings qw(redefine prototype);
*webkitdirs::isAppleCocoaWebKit = sub () { 1 };
use warnings qw(redefine prototype);

my $configuration = "Debug";
my $base = tempdir(CLEANUP => 1);

# Enable ASan via the marker file that readSanitizerConfiguration() reads.
open(my $asanFh, ">", File::Spec->catfile($base, "ASan")) or die "Could not create ASan marker: $!";
print $asanFh "YES\n";
close($asanFh);

# The CMake ASan build lands in cmake-mac/ASan; Xcode toggles ASan within the
# Debug tree. Both tree dirs must exist for the tiebreaker to run.
my $cmakeMarker = File::Spec->catfile($base, "cmake-mac", "ASan", ".ninja_log");
my $xcodeMarker = File::Spec->catfile($base, "XCBuildData", "build.db");
make_path(File::Spec->catdir($base, $configuration));

for my $marker ($cmakeMarker, $xcodeMarker) {
    my ($volume, $dir, $file) = File::Spec->splitpath($marker);
    make_path(File::Spec->catpath($volume, $dir, ""));
    open(my $fh, ">", $marker) or die "Could not create $marker: $!";
    close($fh);
}

# Make the CMake ASan build log the more recently built one.
my $now = time();
utime($now - 100, $now - 100, $xcodeMarker);
utime($now, $now, $cmakeMarker);

setBaseProductDir($base);
setConfiguration($configuration);
enableLastBuiltTiebreaker();

@ARGV = ();
webkitdirs::determineIsCMakeBuild();

ok(isCMakeBuild(), "last-built tiebreaker inspects cmake-mac/ASan when ASan is enabled");
