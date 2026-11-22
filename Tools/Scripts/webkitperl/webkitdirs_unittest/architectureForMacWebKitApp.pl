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

# Unit test for webkitdirs::architectureForMacWebKitApp, which picks the
# architecture run-safari and debug-safari launch an app with.
use strict;
use warnings;
use File::Spec;
use File::Temp qw(tempdir);
use Test::More;
use webkitdirs;

plan(tests => 12);

my $buildArchitecture;
my %slicesForExecutable;
my @executablesInspected;
my @bundlesResolved;

no warnings qw(redefine prototype);
*webkitdirs::architecture = sub () { $buildArchitecture };
*webkitdirs::executableArchitectures = sub ($) {
    my ($executablePath) = @_;
    push @executablesInspected, $executablePath;
    return @{$slicesForExecutable{$executablePath} || []};
};
*webkitdirs::appExecutablePathFromBundle = sub ($) {
    my ($appBundle) = @_;
    push @bundlesResolved, $appBundle;
    return File::Spec->catfile($appBundle, "Contents", "MacOS", "App");
};
use warnings qw(redefine prototype);

sub architectureFor($$)
{
    my ($architecture, $appPath) = @_;
    $buildArchitecture = $architecture;
    @executablesInspected = ();
    @bundlesResolved = ();
    return webkitdirs::architectureForMacWebKitApp($appPath);
}

%slicesForExecutable = (
    "/apps/arm64-x86_64" => ["arm64", "x86_64"],
    "/apps/arm64e-x86_64" => ["arm64e", "x86_64"],
    "/apps/x86_64" => ["x86_64"],
);

is(architectureFor("arm64e", "/apps/arm64-x86_64"), "arm64", "an arm64e build launches the arm64 slice of an app without arm64e");
is(architectureFor("arm64e", "/apps/arm64e-x86_64"), "arm64e", "an arm64e build launches the arm64e slice of an app that has one");
is(architectureFor("arm64e", "/apps/x86_64"), "arm64e", "an arm64e build keeps arm64e for an app without an arm64 slice");
is_deeply(\@bundlesResolved, [], "an executable path is used as is");

is(architectureFor("arm64", "/apps/arm64-x86_64"), "arm64", "an arm64 build launches arm64");
is_deeply(\@executablesInspected, [], "an arm64 build does not inspect the app");
is(architectureFor("x86_64", "/apps/arm64-x86_64"), "x86_64", "an x86_64 build launches x86_64");

my @warnings;
{
    local $SIG{__WARN__} = sub { push @warnings, @_ };
    is(architectureFor(undef, "/apps/arm64-x86_64"), undef, "an unknown build architecture is returned as is");
}
is_deeply(\@warnings, [], "an unknown build architecture does not warn");

my $bundle = tempdir(CLEANUP => 1);
my $bundleExecutable = File::Spec->catfile($bundle, "Contents", "MacOS", "App");
%slicesForExecutable = ($bundleExecutable => ["arm64", "x86_64"]);

is(architectureFor("arm64e", $bundle), "arm64", "an arm64e build launches the arm64 slice of a bundle's executable without arm64e");
is_deeply(\@bundlesResolved, [$bundle], "a bundle directory is resolved to its executable");
is_deeply(\@executablesInspected, [$bundleExecutable], "the bundle's executable is inspected");
