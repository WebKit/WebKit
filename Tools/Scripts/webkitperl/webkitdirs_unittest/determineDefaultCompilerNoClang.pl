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

# Unit test for webkitdirs::determineDefaultCompiler when the Clang C++ driver is
# missing: both clang and clang++ are required, otherwise the build keeps using the
# default system compiler.
# The function caches its answer in a script-global, so each scenario
# lives in its own .pl file (test-webkitperl runs each in a fresh process).
use strict;
use warnings;
use File::Spec;
use File::Temp;
use Test::More;
use webkitdirs;

# The PATH is replaced below to control which compilers are found, so 'which'
# (used by webkitdirs::commandExists) has to be made available there too.
chomp(my $whichPath = `/bin/sh -c 'command -v which' 2>/dev/null`);
if (!$whichPath || !-x $whichPath) {
    plan(skip_all => "the 'which' command was not found");
}

plan(tests => 3);

my $binDir = File::Temp->newdir();
symlink($whichPath, File::Spec->catfile($binDir, "which")) or die "Failed to link which: $!";
# Only the C compiler is available, the C++ one is missing.
my $clangPath = File::Spec->catfile($binDir, "clang");
open(my $handle, ">", $clangPath) or die "Failed to create $clangPath: $!";
print $handle "#!/bin/sh\n";
close($handle);
chmod(0755, $clangPath) or die "Failed to make $clangPath executable: $!";

$ENV{'PATH'} = "$binDir";
delete $ENV{'CC'};
delete $ENV{'CXX'};

@ARGV = ("--gtk");
my $warning = "";
{
    local *STDERR;
    open(STDERR, ">", \$warning);
    webkitdirs::determineDefaultCompiler();
}

ok(!defined $ENV{'CC'}, "CC is left unset when clang++ is not available");
ok(!defined $ENV{'CXX'}, "CXX is left unset when clang++ is not available");
like($warning, qr/clang\+\+ not found in PATH/, "a warning about the missing clang++ is printed");
