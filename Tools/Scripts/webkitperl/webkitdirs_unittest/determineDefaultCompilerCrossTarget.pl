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

# Unit test for webkitdirs::determineDefaultCompiler with --cross-target. The Clang
# used on those builds is the one from the cross-toolchain, so CC and CXX are set as
# a hint for the toolchain environment script without looking for Clang in the PATH.
# The function caches its answer in a script-global, so each scenario
# lives in its own .pl file (test-webkitperl runs each in a fresh process).
use strict;
use warnings;
use File::Spec;
use File::Temp;
use Test::More;
use webkitdirs;

my $helper = File::Spec->catfile(webkitdirs::sourceDir(), "Tools", "Scripts", "cross-toolchain-helper");
chomp(my @targets = `'$helper' --print-available-targets 2>/dev/null`);
if (!@targets) {
    plan(skip_all => "no cross-targets available");
}

plan(tests => 2);

@ARGV = ("--wpe", "--cross-target=$targets[0]");
delete $ENV{'CC'};
delete $ENV{'CXX'};

# Resolve the cross-target now (it runs cross-toolchain-helper), so that the PATH can be
# emptied below to check that the Clang from the host is not the one taken into account.
webkitdirs::shouldBuildForCrossTarget() or die "Failed to select the cross-target $targets[0]";
my $emptyDir = File::Temp->newdir();
$ENV{'PATH'} = "$emptyDir";

{
    # Keep the informational message out of the TAP stream.
    local *STDOUT;
    open(STDOUT, ">", File::Spec->devnull());
    webkitdirs::determineDefaultCompiler();
}

is($ENV{'CC'}, "clang", "CC is set to clang for cross-target builds");
is($ENV{'CXX'}, "clang++", "CXX is set to clang++ for cross-target builds");
