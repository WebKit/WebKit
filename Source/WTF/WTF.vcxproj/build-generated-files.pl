#!/usr/bin/perl -w

# Copyright (C) 2007, 2014 Apple Inc.  All rights reserved.
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
# 3.  Neither the name of Apple puter, Inc. ("Apple") nor the names of
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

use strict;
use File::Path qw(make_path);
use File::Spec;
use File::stat;

# Make sure we don't have any leading or trailing quotes
for (@ARGV) {
    s/^\"//;
    s/\"$//;
}

# Determine whether we have the versioned ICU 4.0 or the unversioned ICU 4.4
my $UNVERSIONED_ICU_LIB_PATH = File::Spec->catfile($ARGV[1], "lib$ARGV[3]", "libicuuc$ARGV[2].lib");
my $PRIVATE_INCLUDE =  File::Spec->catdir($ARGV[0], 'include', 'private');
my $ICUVERSION_H_PATH = File::Spec->catfile($PRIVATE_INCLUDE, 'ICUVersion.h');
if ((! -f $ICUVERSION_H_PATH) or (-f $UNVERSIONED_ICU_LIB_PATH and (stat($UNVERSIONED_ICU_LIB_PATH)->mtime > stat($ICUVERSION_H_PATH)->mtime))) {
    unless (-d $PRIVATE_INCLUDE) {
        make_path($PRIVATE_INCLUDE) or die "Couldn't create $PRIVATE_INCLUDE: $!";
    }

    my $disableRenaming = (-f $UNVERSIONED_ICU_LIB_PATH) ? 1 : 0;
    open(ICU_VERSION_FILE, '>', $ICUVERSION_H_PATH) or die "Unable to open $ICUVERSION_H_PATH: $!";
    print ICU_VERSION_FILE "#define U_DISABLE_RENAMING $disableRenaming\n";
    close(ICU_VERSION_FILE);
}
