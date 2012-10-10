#! /usr/bin/perl
#
#   This file is part of the WebKit project
#
#   Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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
use File::Basename;
use File::Spec;
use Getopt::Long;
use IO::File;

require Config;

my $gcc = "";
if ($ENV{CC}) {
    $gcc = $ENV{CC};
} elsif (($Config::Config{'osname'}) =~ /solaris/i) {
    $gcc = "/usr/sfw/bin/gcc";
} else {
    $gcc = "/usr/bin/gcc";
}

$gcc .= " -E -P -x c";

my $grammarFilePath = "";
my $outputDir = ".";
my $extraDefines = "";
my $symbolsPrefix = "";

GetOptions(
    'grammar=s' => \$grammarFilePath,
    'outputDir=s' => \$outputDir,
    'preprocessor=s' => \$gcc,
    'extraDefines=s' => \$extraDefines,
    'symbolsPrefix=s' => \$symbolsPrefix
);

die "Need a symbols prefix to give to bison (e.g. cssyy, xpathyy)" unless length($symbolsPrefix);

my ($filename, $basePath, $suffix) = fileparse($grammarFilePath, (".y", ".y.in"));

if ($suffix eq ".y.in") {
    my $grammarFileIncludesPath = "${basePath}${filename}.y.includes";
    my $grammarFileOutPath = File::Spec->join($outputDir, "$filename.y");
    my $featureFlags = "-D " . join(" -D ", split(" ", $extraDefines));

    my $processed = new IO::File;
    open $processed, "$gcc $grammarFilePath $featureFlags|";

    open GRAMMAR, ">$grammarFileOutPath" or die;
    open INCLUDES, "<$grammarFileIncludesPath" or die;

    while (<INCLUDES>) {
        print GRAMMAR;
    }
    while (<$processed>) {
        print GRAMMAR;
    }

    close GRAMMAR;
    $grammarFilePath = $grammarFileOutPath;
}

my $fileBase = File::Spec->join($outputDir, $filename);
system("bison -d -p $symbolsPrefix $grammarFilePath -o $fileBase.cpp");

open HEADER, ">$fileBase.h" or die;
print HEADER << "EOF";
#ifndef CSSGRAMMAR_H
#define CSSGRAMMAR_H
EOF

open HPP, "<$fileBase.cpp.h" or open HPP, "<$fileBase.hpp" or die;
while (<HPP>) {
    print HEADER;
}
close HPP;

print HEADER "#endif\n";
close HEADER;

unlink("$fileBase.cpp.h");
unlink("$fileBase.hpp");

