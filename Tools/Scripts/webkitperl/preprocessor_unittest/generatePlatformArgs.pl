#!/usr/bin/env perl

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


# Unit tests for the define names Source/WTF/Scripts/generate-platform-args passes to the generators and Swift.

use strict;
use warnings;

use File::Spec;
use File::Temp qw(tempdir);
use FindBin;
use Test::More;

my $generatePlatformArgs = File::Spec->catfile($FindBin::Bin, "..", "..", "..", "..", "Source", "WTF", "Scripts", "generate-platform-args");
my $platformHeaders = File::Spec->catdir($FindBin::Bin, "resources", "platform");

my $clang = `/bin/sh -c 'command -v clang' 2>/dev/null`;
chomp($clang);

sub securityAccelerationNames
{
    my $names = shift;
    return [sort grep { /^(RC_REMOVE_SEC_ACCEL_|LOG_FOO|UNRELATED_NAME)/ } keys %$names];
}

SKIP: {
    skip "clang is not installed", 6 unless $clang;

    my @clangCommand = ($clang, "-x", "c++", "-E", "-P", "-dM", "-I", $platformHeaders, "-include", "wtf/Platform.h",
        "-DRC_REMOVE_SEC_ACCEL_5", "-DLOG_FOO=1", "-DUNRELATED_NAME=1", "/dev/null");

    my $command = join(" ", map { "'$_'" } ($generatePlatformArgs, "--print-feature-defines", "--", @clangCommand));
    my %names = map { $_ => 1 } split(/\n/, `$command 2>/dev/null`);
    is($? >> 8, 0, "generate-platform-args --print-feature-defines succeeds");
    is_deeply(securityAccelerationNames(\%names), ["LOG_FOO", "RC_REMOVE_SEC_ACCEL_5"],
        "RC_REMOVE_SEC_ACCEL_ names reach the generators");

    my $outputDirectory = tempdir(CLEANUP => 1);
    my $cmakeconfig = File::Spec->catfile($outputDirectory, "cmakeconfig.h");
    my $response = File::Spec->catfile($outputDirectory, "platform-swift-args.resp");
    open(my $fh, ">", $cmakeconfig) or die "Cannot write $cmakeconfig";
    print $fh "#define ENABLE_CMAKE_FEATURE 1\n";
    close($fh);
    $command = join(" ", map { "'$_'" } ($generatePlatformArgs, "--cmake", "--output", $response, "--cmakeconfig", $cmakeconfig, "--", @clangCommand));
    `$command 2>/dev/null`;
    is($? >> 8, 0, "generate-platform-args --cmake succeeds");
    open($fh, "<", $response) or die "Cannot read $response";
    %names = map { chomp; /^-D(\w+)$/ ? ($1 => 1) : () } <$fh>;
    close($fh);
    is_deeply(securityAccelerationNames(\%names), ["LOG_FOO", "RC_REMOVE_SEC_ACCEL_5"], "--cmake passes RC_REMOVE_SEC_ACCEL_ names to Swift");

    SKIP: {
        skip "xcrun is not installed", 2 unless -x "/usr/bin/xcrun";

        $outputDirectory = tempdir(CLEANUP => 1);
        local $ENV{ARCHS} = "arm64";
        local $ENV{BUILT_PRODUCTS_DIR} = $platformHeaders;
        local $ENV{CLANG_CXX_LANGUAGE_STANDARD} = "c++2b";
        local $ENV{DERIVED_FILES_DIR} = $outputDirectory;
        local $ENV{GCC_PREPROCESSOR_DEFINITIONS} = "RC_REMOVE_SEC_ACCEL_5 LOG_FOO=1 UNRELATED_NAME=1";
        local $ENV{LLVM_TARGET_TRIPLE_OS_VERSION} = "macos14.0";
        local $ENV{LLVM_TARGET_TRIPLE_VENDOR} = "apple";
        local $ENV{SCRIPT_OUTPUT_FILE_0} = File::Spec->catfile($outputDirectory, "platform-enabled-swift-args.arm64.resp");
        `'$generatePlatformArgs' 2>/dev/null`;
        is($? >> 8, 0, "generate-platform-args in an Xcode build succeeds");
        open($fh, "<", $ENV{SCRIPT_OUTPUT_FILE_0}) or die "Cannot read $ENV{SCRIPT_OUTPUT_FILE_0}";
        %names = map { chomp; s/^-D//r => 1 } <$fh>;
        close($fh);
        is_deeply(securityAccelerationNames(\%names), ["LOG_FOO", "RC_REMOVE_SEC_ACCEL_5"],
            "an Xcode build passes RC_REMOVE_SEC_ACCEL_ names to Swift and the generators");
    }
}

done_testing();
