# Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
# 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

# Module to share code to get to WebKit directories.

use strict;
use warnings;

BEGIN {
   use Exporter   ();
   our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
   $VERSION     = 1.00;
   @ISA         = qw(Exporter);
   @EXPORT      = qw(&chdirWebKit &productDir);
   %EXPORT_TAGS = ( );
   @EXPORT_OK   = ();
}

our @EXPORT_OK;

# Check that we're in the right directory.
sub chdirWebKit
{
    if (! -d "WebKitTools") {
        if (-d "../WebKitTools") {
            chdir ".." or die;
        }
        if (-d "../../WebKitTools") {
            chdir "../.." or die;
        }
        if (! -d "WebKitTools") {
            die "No WebKitTools directory found. Please run this script from the directory containing WebKitTools.\n";
        }
    }
}

# Check that an Xcode product directory is set.
sub productDir
{
    open PRODUCT, "defaults read com.apple.Xcode PBXProductDirectory 2> /dev/null |" or die;
    my $productDir = <PRODUCT>;
    chomp $productDir;
    close PRODUCT;
    $productDir = "~/WebKitBuilds" unless $productDir;
    $productDir =~ s|^~/|$ENV{HOME}/|;
    return $productDir;
}

1;
