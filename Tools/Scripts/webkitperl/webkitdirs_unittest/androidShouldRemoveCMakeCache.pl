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

# Unit test for webkitdirs::shouldRemoveCMakeCache on an Android build: the CMake
# arguments for Android are not given to build-webkit, so the build has to be
# configured again when they change, for example when other dependencies are used.
use strict;
use warnings;
use File::Path qw(make_path);
use File::Spec;
use File::Temp qw(tempdir);
use Test::More;
use webkitdirs;

if (!webkitdirs::isLinux()) {
    plan(skip_all => "Android builds are only supported on Linux");
}

plan(tests => 3);

setBaseProductDir(tempdir(CLEANUP => 1));
delete $ENV{'WEBKIT_OUTPUTDIR'};
delete $ENV{'BUILD_WEBKIT_ARGS'};
@ARGV = ("--android", "--release");
setConfiguration();

my $directory = webkitdirs::androidDependenciesDirectory();
make_path($directory);
symlink(tempdir(CLEANUP => 1), File::Spec->catdir($directory, "ndk")) or die;

# Points the link to new dependencies, as update-webkit-android-libs does for each version.
sub linkNewDependencies()
{
    my $link = File::Spec->catdir($directory, "dependencies-arm64-v8a");
    unlink($link);
    symlink(tempdir(CLEANUP => 1), $link) or die;
}

sub shouldRemoveCMakeCache()
{
    # Keep the report of the arguments that changed out of the TAP stream.
    local *STDOUT;
    open(STDOUT, ">", File::Spec->devnull());
    return webkitdirs::shouldRemoveCMakeCache();
}

linkNewDependencies();
ok(shouldRemoveCMakeCache(), "a fresh build is configured");

# Without a CMake cache there is nothing left to remove when nothing changed.
ok(!shouldRemoveCMakeCache(), "the build is not configured again when nothing changed");

linkNewDependencies();
ok(shouldRemoveCMakeCache(), "the build is configured again for other dependencies");
