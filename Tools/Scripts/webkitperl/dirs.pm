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

package webkitperl::dirs;

use strict;
use warnings;
use Carp;
use File::Spec;

BEGIN {
   use Exporter   ();
   our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
   $VERSION     = 1.00;
   @ISA         = qw(Exporter);
   @EXPORT      = qw(
                    sourceDir
                    setSourceDir
                );
   %EXPORT_TAGS = ();
   @EXPORT_OK   = ();
}

my $sourceDir;

sub sourceDir
{
    determineSourceDir();
    return $sourceDir;
}

# used for scripts which are stored in a non-standard location
sub setSourceDir($)
{
    ($sourceDir) = @_;
}

sub determineSourceDir
{
    return if $sourceDir;
    $sourceDir = $FindBin::RealBin;
    $sourceDir =~ s|/+$||; # Remove trailing '/' as we would die later

    # walks up path checking each directory to see if it is the main WebKit project dir,
    # defined by containing Sources, WebCore, and JavaScriptCore.
    until ((-d File::Spec->catdir($sourceDir, "Source") && -d File::Spec->catdir($sourceDir, "Source", "WebCore") && -d File::Spec->catdir($sourceDir, "Source", "JavaScriptCore")) || (-d File::Spec->catdir($sourceDir, "Internal") && -d File::Spec->catdir($sourceDir, "OpenSource")))
    {
        if ($sourceDir !~ s|/[^/]+$||) {
            croak "Could not find top level webkit directory above source directory using FindBin.";
        }
    }

    $sourceDir = File::Spec->catdir($sourceDir, "OpenSource") if -d File::Spec->catdir($sourceDir, "OpenSource");
}

1;
