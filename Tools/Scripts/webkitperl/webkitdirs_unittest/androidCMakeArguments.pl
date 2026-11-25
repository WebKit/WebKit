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

# Unit test for the CMake arguments of an Android build, which take the NDK and the
# dependencies that update-webkit-android-libs links to.
use strict;
use warnings;
use Cwd qw(realpath);
use File::Path qw(make_path);
use File::Spec;
use File::Temp qw(tempdir);
use Test::More;
use webkitdirs;

if (!webkitdirs::isLinux()) {
    plan(skip_all => "Android builds are only supported on Linux");
}

plan(tests => 7);

setBaseProductDir(tempdir(CLEANUP => 1));
@ARGV = ("--android");

my $directory = webkitdirs::androidDependenciesDirectory();
my $ndkLink = File::Spec->catdir($directory, "ndk");
my $dependenciesLink = File::Spec->catdir($directory, "dependencies-arm64-v8a");
is(webkitdirs::androidNDKPath(), $ndkLink, "the NDK is the link of update-webkit-android-libs before that runs");
is(androidDependenciesPath(), $dependenciesLink, "so are the dependencies");

my $ndk = realpath(tempdir(CLEANUP => 1));
make_path(File::Spec->catdir($ndk, "build", "cmake"));
my $dependencies = realpath(tempdir(CLEANUP => 1));
make_path($directory);
symlink($ndk, $ndkLink) or die;
symlink($dependencies, $dependenciesLink) or die;

my @arguments = webkitdirs::androidCMakeArguments();
my $toolchainFile = File::Spec->catfile($ndk, "build", "cmake", "android.toolchain.cmake");
ok((grep { $_ eq "-DCMAKE_TOOLCHAIN_FILE=\"$toolchainFile\"" } @arguments), "the toolchain file of the NDK that the link points to is used");
ok((grep { $_ eq "-DANDROID_ABI=arm64-v8a" } @arguments), "the arm64 architecture is the arm64-v8a ABI");
ok((grep { $_ eq "-DCMAKE_FIND_ROOT_PATH=\"$dependencies\"" } @arguments), "the dependencies are searched for where the link points to");
ok((grep { $_ eq "-DENABLE_WEBXR=ON" } @arguments), "WebXR is enabled, as wpe-android does");

my $otherDependencies = realpath(tempdir(CLEANUP => 1));
unlink($dependenciesLink);
symlink($otherDependencies, $dependenciesLink) or die;
ok((grep { $_ eq "-DCMAKE_FIND_ROOT_PATH=\"$otherDependencies\"" } webkitdirs::androidCMakeArguments()), "the CMake arguments change with what the links point to");
