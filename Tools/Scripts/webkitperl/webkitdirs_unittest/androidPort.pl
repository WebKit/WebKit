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

# Unit test for --android, which builds the WPE port for an arm64 Android device
# by default. The port and the architecture are cached in script-globals, so each
# scenario lives in its own .pl file (test-webkitperl runs each in a fresh process).
use strict;
use warnings;
use File::Spec;
use File::Temp qw(tempdir);
use Test::More;
use webkitdirs;

if (!webkitdirs::isLinux()) {
    plan(skip_all => "Android builds are only supported on Linux");
}

plan(tests => 10);

my $base = tempdir(CLEANUP => 1);
setBaseProductDir($base);
delete $ENV{'WEBKIT_OUTPUTDIR'};
delete $ENV{'CC'};
delete $ENV{'CXX'};

@ARGV = ("--android", "--release", "leftover");
setConfiguration();

ok(isAndroid(), "--android is detected");
ok(isWPE(), "--android builds the WPE port");
is_deeply(\@ARGV, ["leftover"], "--android is removed from \@ARGV");
is(architecture(), "arm64", "the architecture is arm64 by default, whatever the host is");
is(productDir(), File::Spec->catdir($base, "Android-arm64", "Release"), "the build directory is not the one of the WPE port for the host");

# build-webkit enters the container SDK and its private mount namespace by running
# itself again with these arguments, after --android was removed from @ARGV.
my @arguments = argumentsForConfiguration();
ok((grep { $_ eq "--android" } @arguments), "--android is passed on to the commands run again");
ok((grep { $_ eq "--architecture=arm64" } @arguments), "the architecture is passed on to the commands run again");

ok(webkitdirs::isCrossCompilation(), "an Android build is a cross-build");
is_deeply([wrapperPrefixIfNeeded()], [], "an Android build does not run in JHBuild");

{
    # Keep the informational message out of the TAP stream.
    local *STDOUT;
    open(STDOUT, ">", File::Spec->devnull());
    determineDefaultCompiler();
}
ok(!defined($ENV{'CC'}) && !defined($ENV{'CXX'}), "the compiler is left to the toolchain of the NDK");
