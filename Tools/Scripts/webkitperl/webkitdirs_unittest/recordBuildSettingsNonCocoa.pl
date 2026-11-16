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

# Unit test for webkitdirs::recordBuildSettings on a port that only builds with
# CMake: --xcode is accepted and dropped, but no BuildSystem marker is written,
# so it cannot send a later Cocoa build down the Xcode path.
use strict;
use warnings;
use File::Spec;
use File::Temp qw(tempdir);
use Test::More;
use webkitdirs;

plan(tests => 4);

no warnings qw(redefine prototype);
*webkitdirs::isAppleCocoaWebKit = sub () { 0 };
use warnings qw(redefine prototype);

my $base = tempdir(CLEANUP => 1);
setBaseProductDir($base);

@ARGV = ("--debug", "--xcode", "leftover");
setConfiguration();
recordBuildSettings();

is_deeply(\@ARGV, ["leftover"], "the recorded arguments are removed from \@ARGV");

sub settingIs($)
{
    my ($fileName) = @_;
    open(my $fh, "<", File::Spec->catfile($base, $fileName)) or return undef;
    my $value = <$fh>;
    close($fh);
    chomp $value if defined $value;
    return $value;
}

is(settingIs("Configuration"), "Debug", "--debug is recorded");
is(settingIs("BuildSystem"), undef, "the build system is not recorded");

# A marker left behind by a Cocoa build in the same checkout does not send this
# port down the Xcode path either.
writeBuildSetting("BuildSystem", "Xcode");
ok(isCMakeBuild(), "the build stays a CMake one");
