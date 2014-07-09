#!/usr/bin/perl -w

# Copyright (C) 2007, 2008, 2009, 2010, 2014 Apple Inc. All rights reserved.
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

# Not all build environments have the webkitdirs module installed.
my $NUMCPUS = 2;
eval "use webkitdirs";
unless ($@) {
    $NUMCPUS = numberOfCPUs();
}

# Make sure we don't have any leading or trailing quotes
$ARGV[0] =~ s/^\"//;
$ARGV[0] =~ s/\"$//;

my $MIGRATION_DIR = File::Spec->canonpath($ARGV[0]);
unless (-d $MIGRATION_DIR) {
    make_path($MIGRATION_DIR) or die "Couldn't create $MIGRATION_DIR: $!";
}
$MIGRATION_DIR = Cwd::realpath($MIGRATION_DIR);

my $PWD = Cwd::cwd();
my $XSRCROOT =  Cwd::realpath(File::Spec->updir);
$ENV{'XSRCROOT'} = $XSRCROOT;
$ENV{'SOURCE_ROOT'} = $XSRCROOT;

my $XDSTROOT = Cwd::realpath($ARGV[0]);
$ENV{'XDSTROOT'} = $XDSTROOT;

$ENV{'WEBCORE'} = $XSRCROOT;
$ENV{'WebCore'} = $XSRCROOT;
$ENV{'WEBKIT_OUTPUT'} = $XDSTROOT;

my $MIGRATE_SCRIPTS_MAKEFILE = File::Spec->catfile($XSRCROOT, 'WebCore.vcxproj', 'MigrateScripts');
system('/usr/bin/make', '-f', $MIGRATE_SCRIPTS_MAKEFILE, '-j', $NUMCPUS) and die "Failed to build $MIGRATE_SCRIPTS_MAKEFILE: $!";
