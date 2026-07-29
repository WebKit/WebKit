#!/usr/bin/env perl
#
# Copyright (C) 2026 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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
use File::Basename;
use Getopt::Long;

require JSON::PP;

my $idlFilesList;
my $outputFile;

GetOptions(
    'idlFilesList=s' => \$idlFilesList,
    'output=s' => \$outputFile,
);

my $inputDirectory = dirname($outputFile);

sub readFile
{
    my $path = shift;
    local $/;
    open my $fh, "<", $path or return undef;
    my $contents = <$fh>;
    close $fh;
    return $contents;
}

my $json = JSON::PP->new->utf8;
my %output;

open my $listFh, "<", $idlFilesList or die "Couldn't open $idlFilesList: $!\n";
while (my $line = <$listFh>) {
    chomp $line;
    foreach my $path (split ' ', $line) {
        next unless $path =~ /\.idl$/;

        my $basename = fileparse($path, ".idl");
        my $filename = "${basename}.inspector-native-function-parameters.json";
        my $contents = readFile("${inputDirectory}/${filename}");
        next unless defined $contents;

        my $input = $json->decode($contents);
        foreach my $name (keys %{$input}) {
            my $entry = ($output{$name} ||= {});
            foreach my $kind (keys %{$input->{$name}}) {
                foreach my $method (keys %{$input->{$name}{$kind}}) {
                    next if exists $entry->{$kind}{$method}; # Only keep the first overload.
                    $entry->{$kind}{$method} = $input->{$name}{$kind}{$method};
                }
            }
        }
    }
}
close $listFh;

my $encoded = JSON::PP->new->utf8->canonical->pretty->indent_length(4)->space_before(0)->encode(\%output);

my $existing = readFile($outputFile);
if (!defined $existing || $existing ne $encoded) {
    open my $fh, ">", $outputFile or die "Couldn't open $outputFile: $!\n";
    print $fh $encoded;
    close $fh;
}
