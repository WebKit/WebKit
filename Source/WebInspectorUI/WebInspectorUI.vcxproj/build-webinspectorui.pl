#!/usr/bin/env perl

# Copyright (C) 2014, 2015 Apple Inc.  All rights reserved.
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
use warnings;
use Cwd;
use File::Path qw(make_path);
use File::Spec;

my $PWD = Cwd::cwd();
my $XSRCROOT =  Cwd::realpath(File::Spec->updir);
$ENV{'XSRCROOT'} = $XSRCROOT;
$ENV{'SRCROOT'} = $XSRCROOT;

# Make sure we don't have any leading or trailing quotes
$ARGV[0] =~ s/^\"//;
$ARGV[0] =~ s/\"$//;

my $XDSTROOT = Cwd::realpath($ARGV[0]);
$ENV{'XDSTROOT'} = $XDSTROOT;

my $TARGET_BUILD_DIR = File::Spec->catdir($XDSTROOT, "bin$ARGV[3]", 'WebKit.resources');
$ENV{'TARGET_BUILD_DIR'} = $TARGET_BUILD_DIR;
my $JAVASCRIPTCORE_PRIVATE_HEADERS_DIR;
if ($ARGV[4] eq '1') {
    $ARGV[1] =~ s/^\"//;
    $ARGV[1] =~ s/\"$//;
    my $Internal = Cwd::realpath($ARGV[1]);;
    $JAVASCRIPTCORE_PRIVATE_HEADERS_DIR = File::Spec->catdir($Internal, 'include', 'private', 'JavaScriptCore');
} else {
    $JAVASCRIPTCORE_PRIVATE_HEADERS_DIR = File::Spec->catdir($XDSTROOT, "obj$ARGV[3]", 'JavaScriptCore', 'DerivedSources');
}

$ENV{'JAVASCRIPTCORE_PRIVATE_HEADERS_DIR'} = $JAVASCRIPTCORE_PRIVATE_HEADERS_DIR;

my $DERIVED_SOURCES_DIR = File::Spec->catdir($XDSTROOT, "obj$ARGV[3]", 'WebInspectorUI', 'DerivedSources');
$ENV{'DERIVED_SOURCES_DIR'} = $DERIVED_SOURCES_DIR;

$ENV{'UNLOCALIZED_RESOURCES_FOLDER_PATH'} = 'WebInspectorUI';

if (($TARGET_BUILD_DIR =~ /Release/) || ($TARGET_BUILD_DIR =~ /Production/)) {
    $ENV{'COMBINE_INSPECTOR_RESOURCES'} = 'YES';
}

if ($TARGET_BUILD_DIR !~ /Production/) {
    $ENV{'COMBINE_TEST_RESOURCES'} = 'YES';
}

my $targetResourcePath = File::Spec->catdir($ENV{'TARGET_BUILD_DIR'}, $ENV{'UNLOCALIZED_RESOURCES_FOLDER_PATH'});
my $protocolDir = File::Spec->catdir($targetResourcePath, 'Protocol');

# Copy over dynamically loaded files from other frameworks, even if we aren't combining resources.
my $jsFrom = File::Spec->catfile($ENV{'JAVASCRIPTCORE_PRIVATE_HEADERS_DIR'}, 'InspectorBackendCommands.js');
my $jsTo = File::Spec->catfile($protocolDir, 'InspectorBackendCommands.js');
print "Copying Inspector bindings from $jsFrom to $jsTo\n";

my $copyResourcesCommand = File::Spec->catfile($XSRCROOT, 'Scripts', 'copy-user-interface-resources.pl');
do $copyResourcesCommand;
