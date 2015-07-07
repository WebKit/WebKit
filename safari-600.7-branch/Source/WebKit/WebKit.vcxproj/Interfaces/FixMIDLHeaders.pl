#! /usr/bin/perl -w

# Copyright (C) 2007, 2014 Apple Inc. All rights reserved.
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

# This script cleans up the headers that MIDL (Microsoft's IDL Parser/Generator)
# outputs to only #include the parent interface avoiding circular dependencies
# that MIDL creates.

use File::Find;
use strict;
use warnings;

# Make sure we don't have any leading or trailing quotes
$ARGV[0] =~ s/^\"//;
$ARGV[0] =~ s/\"$//;

my $dir = $ARGV[0];

$dir = `cygpath -u '$dir'`;
chomp($dir);

find(\&finder, $dir);

sub finder
{
    my $fileName = $_;

    return unless ($fileName =~ /IGEN_DOM(.*)\.h/);

    open(IN, "<", $fileName);
    my @contents = <IN>;
    close(IN);

    open(OUT, ">", $fileName);

    my $state = 0;
    foreach my $line (@contents) {
        if ($line =~ /^\/\* header files for imported files \*\//) {
            $state = 1;
        } elsif ($line =~ /^#include "oaidl\.h"/) {
            die "#include \"oaidl.h\" did not come second" if $state != 1;
            $state = 2;
        } elsif ($line =~ /^#include "ocidl\.h"/) {
            die "#include \"ocidl.h\" did not come third" if $state != 2;
            $state = 3;
        } elsif ($line =~ /^#include "IGEN_DOM/ && $state == 3) {
            $state = 4;
        } elsif ($line =~ /^#include "(IGEN_DOM.*)\.h"/ && $state == 4) {
            next;
        }

        print OUT $line;
    }

    close(OUT);
}
