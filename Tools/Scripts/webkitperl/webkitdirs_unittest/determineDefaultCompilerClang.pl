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

# Unit test for webkitdirs::determineDefaultCompiler with Clang available.
# The function caches its answer in a script-global, so each scenario
# lives in its own .pl file (test-webkitperl runs each in a fresh process).
use strict;
use warnings;
use File::Spec;
use File::Temp;
use Test::More;
use webkitdirs;

plan(tests => 3);

my $binDir = File::Temp->newdir();
foreach my $program ("clang", "clang++") {
    my $path = File::Spec->catfile($binDir, $program);
    open(my $handle, ">", $path) or die "Failed to create $path: $!";
    print $handle "#!/bin/sh\n";
    close($handle);
    chmod(0755, $path) or die "Failed to make $path executable: $!";
}

$ENV{'PATH'} = $binDir . ":" . $ENV{'PATH'};
delete $ENV{'CC'};
delete $ENV{'CXX'};

@ARGV = ("--gtk", "leftover");
{
    # Keep the informational message out of the TAP stream.
    local *STDOUT;
    open(STDOUT, ">", File::Spec->devnull());
    webkitdirs::determineDefaultCompiler("-DCMAKE_CXX_COMPILER_LAUNCHER=ccache");
}

is($ENV{'CC'}, "clang", "CC defaults to clang");
is($ENV{'CXX'}, "clang++", "CXX defaults to clang++");
is_deeply(\@ARGV, ["leftover"], "unrelated arguments are left in \@ARGV");
