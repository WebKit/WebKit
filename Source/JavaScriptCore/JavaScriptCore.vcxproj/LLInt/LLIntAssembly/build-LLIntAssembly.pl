#!/usr/bin/perl -w

# Copyright (C) 2014 Apple Inc.  All rights reserved.
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
use Cwd;
use File::Path qw(make_path);
use File::Spec;

my $PWD = Cwd::cwd();
my $XSRCROOT = Cwd::realpath(File::Spec->catdir($PWD, '..', '..', '..'));
$ENV{'XSRCROOT'} = $XSRCROOT;
$ENV{'SRCROOT'} = $XSRCROOT;

# Make sure we don't have any leading or trailing quotes
$ARGV[0] =~ s/^\"//;
$ARGV[0] =~ s/\"$//;

my $XDSTROOT = Cwd::realpath($ARGV[0]);
$ENV{'XDSTROOT'} = $XDSTROOT;

my $BUILD_PRODUCTS_DIR = File::Spec->catdir($XDSTROOT, "obj$ARGV[3]");
$ENV{'BUILT_PRODUCTS_DIR'} = $BUILD_PRODUCTS_DIR;

my $DERIVED_SOURCES_DIR = File::Spec->catdir($BUILD_PRODUCTS_DIR, 'JavaScriptCore', 'DerivedSources');
unless (-d $DERIVED_SOURCES_DIR) {
    make_path($DERIVED_SOURCES_DIR) or die "Couldn't create $DERIVED_SOURCES_DIR: $!";
}

chdir $DERIVED_SOURCES_DIR or die "Couldn't change directory to $DERIVED_SOURCES_DIR: $!";

# Create a dummy asm file in case we are using the C backend
# This is needed since LowLevelInterpreterWin.asm is part of the project.
my $LowLevelInterpreterWin = File::Spec->catfile($DERIVED_SOURCES_DIR, 'LowLevelInterpreterWin.asm');

open(OUTPUTFILENAME, '>', $LowLevelInterpreterWin) or die "Couldn't open $LowLevelInterpreterWin to write: $!";
print OUTPUTFILENAME "END\n";
close(OUTPUTFILENAME);

# If you want to enable the LLINT C loop, set OUTPUTFILENAME to "LLIntAssembly.h"
my $OUTPUTFILENAME = File::Spec->catfile($DERIVED_SOURCES_DIR, 'LowLevelInterpreterWin.asm');

my $offlineAsm = File::Spec->catfile($XSRCROOT, 'offlineasm', 'asm.rb');
my $lowLevelInterpreter = File::Spec->catfile($XSRCROOT, 'llint', 'LowLevelInterpreter.asm');
my $offsetsExtractor = File::Spec->catfile($BUILD_PRODUCTS_DIR, 'LLIntOffsetsExtractor', "LLIntOffsetsExtractor$ARGV[2].exe");
system('/usr/bin/env', 'ruby', $offlineAsm, '-I.', $lowLevelInterpreter, $offsetsExtractor, $OUTPUTFILENAME) and die "Failed to generate $OUTPUTFILENAME: $!";
