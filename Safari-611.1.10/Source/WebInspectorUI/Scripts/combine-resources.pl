#!/usr/bin/env perl

# Copyright (C) 2015-2018 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

use strict;
use warnings;
use Getopt::Long;
use File::Basename;
use File::Path;

our $inputDirectory;
our $outputDirectory;
our $outputScriptName;
our $outputStylesheetName;
our $derivedSourcesDirectory;
our $htmlDirectory;
our $htmlFile;
our $strip;
our $verbose;

GetOptions('output-dir=s' => \$outputDirectory,
           'output-script-name=s' => \$outputScriptName,
           'output-style-name=s' => \$outputStylesheetName,
           'derived-sources-dir=s' => \$derivedSourcesDirectory,
           'input-dir=s' => \$inputDirectory,
           'input-html-dir=s' => \$htmlDirectory,
           'input-html=s' => \$htmlFile,
           'verbose' => \$verbose,
           'strip' => \$strip);

unless (defined $htmlFile and defined $derivedSourcesDirectory and defined $outputDirectory and (defined $strip or defined $outputScriptName or defined $outputStylesheetName)) {
    print "Usage: $0 --input-html <path> --derived-sources-dir <path> --output-dir <path> [--output-script-name <name>] [--output-style-name <name>] [--strip]\n";
    exit;
}

sub debugLog($)
{
    my $logString = shift;
    print "-- $logString\n" if $verbose;
}

$htmlDirectory = dirname($htmlFile) unless $htmlDirectory;

our $htmlContents;

{
    local $/;
    open HTML, $htmlFile or die "Could not open $htmlFile";
    $htmlContents = <HTML>;
    close HTML;
}

$htmlContents =~ m/<head>(.*)<\/head>/si;
our $headContents = $1;

mkpath $outputDirectory;

sub concatenateIncludedFilesMatchingPattern($$$)
{
    my $filename = shift;
    my $tagExpression = shift;
    my $concatenatedTag = shift;
    my $fileCount = 0;

    debugLog("combining files for $filename with pattern $tagExpression");

    open OUT, ">", "$outputDirectory/$filename" or die "Can't open $outputDirectory/$filename: $!";

    while ($headContents =~ m/$tagExpression/gi) {
        local $/;
        open IN, "$htmlDirectory/$1" or open IN, "$derivedSourcesDirectory/$1" or die "Can't open $htmlDirectory/$1: $!";
        print OUT "\n" if $fileCount++;
        print OUT "/* $1 */\n\n";
        print OUT <IN>;
        close IN;
    }

    close OUT;

    # Don't use \s so we can control the newlines we consume.
    my $replacementExpression = "([\t ]*)" . $tagExpression . "[\t ]*\n+";

    # Replace the first occurrence with a token so we can inject the concatenated tag in the same place
    # as the first file that got consolidated. This makes sure we preserve some order if there are other
    # items in the head that we didn't consolidate.
    $headContents =~ s/$replacementExpression/$1%CONCATENATED%\n/i;
    $headContents =~ s/$replacementExpression//gi;
    $headContents =~ s/%CONCATENATED%/$concatenatedTag/;
}

sub stripIncludedFilesMatchingPattern($)
{
    my $tagPattern = shift;

    # Don't use \s so we can control the newlines we consume.
    my $whitespaceConsumingTagPattern = "([\t ]*)" . $tagPattern . "[\t ]*\n+";
    $headContents =~ s/$whitespaceConsumingTagPattern//gi;
}

my $inputDirectoryPattern = "(?!WebKitAdditions\/)(?!External\/)(?!Workers\/)[^\"]*";
$inputDirectoryPattern = $inputDirectory . "\/[^\"]*" if $inputDirectory;

if (defined($strip)) {
    stripIncludedFilesMatchingPattern("<link rel=\"stylesheet\" href=\"($inputDirectoryPattern)\">");
    stripIncludedFilesMatchingPattern("<script src=\"($inputDirectoryPattern)\"><\/script>");
} else {
    concatenateIncludedFilesMatchingPattern($outputStylesheetName, "<link rel=\"stylesheet\" href=\"($inputDirectoryPattern)\">", "<link rel=\"stylesheet\" href=\"$outputStylesheetName\">") if defined $outputStylesheetName;
    concatenateIncludedFilesMatchingPattern($outputScriptName, "<script src=\"($inputDirectoryPattern)\"><\/script>", "<script src=\"$outputScriptName\"></script>") if defined $outputScriptName;
}

$htmlContents =~ s/<head>.*<\/head>/<head>$headContents<\/head>/si;

open HTML, ">", "$outputDirectory/" . basename($htmlFile) or die "Can't open $outputDirectory/" . basename($htmlFile) . ": $!";
print HTML $htmlContents;
close HTML;
