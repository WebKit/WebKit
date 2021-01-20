#! /usr/bin/env perl
#
#   This file is part of the WebKit project
#
#   Copyright (C) 2020 Apple Inc. All rights reserved.
#
#   This library is free software; you can redistribute it and/or
#   modify it under the terms of the GNU Library General Public
#   License as published by the Free Software Foundation; either
#   version 2 of the License, or (at your option) any later version.
#
#   This library is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#   Library General Public License for more details.
#
#   You should have received a copy of the GNU Library General Public License
#   along with this library; see the file COPYING.LIB.  If not, write to
#   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
#   Boston, MA 02110-1301, USA.

use strict;
use warnings;

use Config;
use Getopt::Long;
use IPC::Open2;
use IPC::Open3;
use Text::ParseWords;

my $inputPath;
my $defines;
my $preprocessor;
my $outputPath;
GetOptions('input=s' => \$inputPath,
           'defines=s' => \$defines,
           'preprocessor=s' => \$preprocessor,
           'output=s' => \$outputPath);

my @args = ();
if (!$preprocessor) {
    if ($Config::Config{"osname"} eq "MSWin32") {
        $preprocessor = $ENV{CC} || "cl";
        push(@args, qw(/nologo /EP /TP /C));
    } else {
        $preprocessor = $ENV{CC} || (-x "/usr/bin/clang" ? "/usr/bin/clang" : "/usr/bin/gcc");
        push(@args, qw(-E -P -x c++ -C));
    }
}

if ($Config::Config{"osname"} eq "darwin") {
    push(@args, "-I" . $ENV{BUILT_PRODUCTS_DIR} . "/usr/local/include") if $ENV{BUILT_PRODUCTS_DIR};
    push(@args, "-isysroot", $ENV{SDKROOT}) if $ENV{SDKROOT};
}

# Remove double quotations from $defines and extract macros.
# For example, if $defines is ' "A=1" "B=1" C=1 ""    D  ',
# then it is converted into four macros -DA=1, -DB=1, -DC=1 and -DD.
$defines =~ s/\"//g;
my @macros = grep { $_ } split(/\s+/, $defines); # grep skips empty macros.
@macros = map { "-D$_" } @macros;

my $pid = 0;
if ($Config{osname} eq "cygwin") {
    $ENV{PATH} = "$ENV{PATH}:/cygdrive/c/cygwin/bin";
    my @preprocessorAndFlags;
    if ($preprocessor eq "/usr/bin/gcc") {
        @preprocessorAndFlags = split(' ', $preprocessor);
    } else {
        $preprocessor =~ /"(.*)"/;
        chomp(my $preprocessor = `cygpath -u '$1'`) if (defined $1);
        chomp($inputPath = `cygpath -w '$inputPath'`);
        @preprocessorAndFlags = ($preprocessor, "/nologo", "/EP", "/TP", "/C");
    }

    # This call can fail if Windows rebases cygwin, so retry a few times until it succeeds.
    for (my $tries = 0; !$pid && ($tries < 20); $tries++) {
        eval {
            # Suppress STDERR so that if we're using cl.exe, the output
            # name isn't needlessly echoed.
            use Symbol 'gensym'; my $err = gensym;
            $pid = open3(\*PP_IN, \*PP_OUT, $err, @preprocessorAndFlags, @args, @macros, $inputPath);
            1;
        } or do {
            sleep 1;
        }
    }
} elsif ($Config::Config{"osname"} eq "MSWin32") {
    # Suppress STDERR so that if we're using cl.exe, the output
    # name isn't needlessly echoed.
    use Symbol 'gensym'; my $err = gensym;
    $pid = open3(\*PP_IN, \*PP_OUT, $err, $preprocessor, @args, @macros, $inputPath);
} else {
    $pid = open2(\*PP_OUT, \*PP_IN, shellwords($preprocessor), @args, @macros, $inputPath);
}
close PP_IN;
my @lines = <PP_OUT>;
close PP_OUT;
waitpid($pid, 0);

open OUTPUT, ">$outputPath" || die "Could not open $outputPath for writing";
foreach my $line (@lines) {
    # Ignore whitespace lines.
    if ($line =~ /^\s*$/) {
        next;
    }

    # Add newlines before comments.
    if ($line =~ /^\/\//) {
        print OUTPUT "\n";
    }

    print OUTPUT $line;
}
close OUTPUT;
