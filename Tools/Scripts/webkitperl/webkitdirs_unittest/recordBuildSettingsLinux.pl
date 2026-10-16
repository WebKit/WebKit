#!/usr/bin/env perl
#
# Copyright (C) 2026 Igalia S.L.
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
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# Unit test for webkitdirs::recordBuildSettings on the ports that keep the builds
# of several ports, configurations and cross-targets in one WebKitBuild directory
# (GTK, WPE and JSCOnly on Linux): nothing given to build-webkit is recorded, so
# the default stays whatever set-webkit-configuration recorded, but the settings
# are still taken out of @ARGV.
use strict;
use warnings;
use File::Spec;
use File::Temp qw(tempdir);
use Test::More;
use webkitdirs;

plan(tests => 5);

no warnings qw(redefine prototype);
*webkitdirs::isAppleCocoaWebKit = sub () { 0 };
*webkitdirs::isWPE = sub () { 1 };
use warnings qw(redefine prototype);

my $base = tempdir(CLEANUP => 1);
setBaseProductDir($base);

# What set-webkit-configuration --debug recorded earlier.
writeBuildSetting("Configuration", "Debug");

@ARGV = ("--release", "--asan", "--force-opt=O3", "leftover");
setConfiguration();
recordBuildSettings();

is_deeply(\@ARGV, ["leftover"], "the settings are removed from \@ARGV");

sub settingIs($)
{
    my ($fileName) = @_;
    open(my $fh, "<", File::Spec->catfile($base, $fileName)) or return undef;
    my $value = <$fh>;
    close($fh);
    chomp $value if defined $value;
    return $value;
}

is(settingIs("Configuration"), "Debug", "--release does not replace the recorded configuration");
is(settingIs("ASan"), undef, "--asan is not recorded");
is(settingIs("ForceOptimizationLevel"), undef, "--force-opt is not recorded");
is(configuration(), "Release", "the configuration given on the command line is still the one used");
