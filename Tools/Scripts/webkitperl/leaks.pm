#!/usr/bin/env perl

# Copyright (C) 2007 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
# 3.  Neither the name of Apple Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Script to run the Mac OS X leaks tool with more expressive '-exclude' lists.

package webkitperl::leaks;

use strict;
use warnings;
use Carp;

use File::Basename;

BEGIN {
   use Exporter   ();
   our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
   $VERSION     = 1.00;
   @ISA         = qw(Exporter);
   @EXPORT      = qw(
                    runLeaks
                    parseLeaksOutput
                );
   %EXPORT_TAGS = ();
   @EXPORT_OK   = ();
}

# Returns the output of the leaks tool in list form.
sub runLeaks($$)
{
    my ($target, $memgraphPath) = @_;

    if (defined $memgraphPath) {
        `/usr/bin/leaks \"$target\" --outputGraph=\"$memgraphPath\"`;
        $target = $memgraphPath;
    }

    # To get a result we can parse, we need to pass --list to all versions of the leaks tool
    # that recognize it, but there is no certain way to tell in advance (the version of the
    # tool is not always tied to OS version, and --help doesn't document the option in some
    # of the tool versions where it's supported and needed).
    my @leaksOutput = `/usr/bin/leaks \"$target\" --list`;

    # FIXME: Remove the fallback once macOS Mojave is the oldest supported version.
    my $leaksExitCode = $? >> 8;
    if ($leaksExitCode > 1) {
        @leaksOutput = `/usr/bin/leaks \"$target\"`;
    }

    return \@leaksOutput;
}

# Returns a list of hash references with the keys { address, size, type, callStack, leaksOutput }
sub parseLeaksOutput(\@)
{
    my ($leaksOutput) = @_;

    # Format:
    #   Process 00000: 1234 nodes malloced for 1234 KB
    #   Process 00000: XX leaks for XXX total leaked bytes.
    #   Leak: 0x00000000 size=1234 [instance of 'blah']
    #       0x00000000 0x00000000 0x00000000 0x00000000 a..d.e.e
    #       ...
    #       Call stack: leak_caller() | leak() | malloc
    #
    #   We treat every line except for  Process 00000: and Leak: as optional

    my $leakCount;
    my @parsedOutput = ();
    my $parsingLeak = 0;
    my $parsedLeakCount = 0;
    for my $line (@$leaksOutput) {
        if ($line =~ /^Process \d+: (\d+) leaks?/) {
            $leakCount = $1;
        }

        if ($line eq "\n") {
            $parsingLeak = 0;
        }

        if ($line =~ /^Leak: /) {
            $parsingLeak = 1;
            $parsedLeakCount++;
            my ($address) = ($line =~ /Leak: ([[:xdigit:]x]+)/);
            if (!defined($address)) {
                croak "Could not parse Leak address.";
            }

            my ($size) = ($line =~ /size=([[:digit:]]+)/);
            if (!defined($size)) {
                croak "Could not parse Leak size.";
            }

            # FIXME: This code seems wrong, the leaks tool doesn't actually use single quotes.
            # We should reconcile with other format changes that happened since, such as the
            # addition of zone information.
            my ($type) = ($line =~ /'([^']+)'/); #'
            if (!defined($type)) {
                $type = ""; # The leaks tool sometimes omits the type.
            }

            my %leak = (
                "address" => $address,
                "size" => $size,
                "type" => $type,
                "callStack" => "", # The leaks tool sometimes omits the call stack.
                "leaksOutput" => $line
            );
            push(@parsedOutput, \%leak);
        } elsif ($parsingLeak) {
            $parsedOutput[$#parsedOutput]->{"leaksOutput"} .= $line;
            if ($line =~ /Call stack:/) {
                $parsedOutput[$#parsedOutput]->{"callStack"} = $line;
            }
        } else {
            my %nonLeakLine = (
                "leaksOutput" => $line
            );
            push(@parsedOutput, \%nonLeakLine);
        }
    }

    if ($parsedLeakCount != $leakCount) {
        croak "Parsed leak count ($parsedLeakCount) does not match leak count reported by leaks tool ($leakCount).";
    }

    return \@parsedOutput;
}

1;
