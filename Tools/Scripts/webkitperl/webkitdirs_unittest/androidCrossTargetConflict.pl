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

# Unit test for --android along with a cross-target build, whose toolchain is not the
# one of the Android NDK. The port is cached in a script-global, so each scenario lives
# in its own .pl file (test-webkitperl runs each in a fresh process).
use strict;
use warnings;
use Test::More;
use webkitdirs;

if (!webkitdirs::isLinux()) {
    plan(skip_all => "Android builds are only supported on Linux");
}

plan(tests => 2);

# build-webkit checks for a cross-target build before it selects the port.
@ARGV = ("--android", "--cross-target=rpi4-64bits-mesa");
eval { webkitdirs::shouldBuildForCrossTarget() };
like($@, qr/^Argument '--android' conflicts with cross-target builds/, "--android cannot be given along with --cross-target");

$ENV{'WEBKIT_CROSS_TARGET'} = "rpi4-64bits-mesa";
eval { portName() };
like($@, qr/^Argument '--android' conflicts with cross-target builds/, "--android cannot build in a cross-target environment");
