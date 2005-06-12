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
use FindBin;

BEGIN {
   use Exporter   ();
   our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
   $VERSION     = 1.00;
   @ISA         = qw(Exporter);
   @EXPORT      = qw(&chdirWebKit &baseProductDir &productDir &XcodeOptions &passedConfiguration &setConfiguration &checkFrameworks);
   %EXPORT_TAGS = ( );
   @EXPORT_OK   = ();
}

our @EXPORT_OK;

my $baseProductDir;
my @baseProductDirOption;
my $configuration;
my $configurationProductDir;
my $didChdirWebKit;
my $XcodeVersion;

# Check that we're in the right directory.
sub chdirWebKit
{
    return if $didChdirWebKit;
    $didChdirWebKit = 1;
    chdir "$FindBin::Bin/../.." or die;
}

sub determineXCodeVersion
{
    return if defined $XcodeVersion;
    # Could use "xcodebuild -version" instead.
    open VERSION, "defaults read /Developer/Applications/Xcode.app/Contents/Info CFBundleShortVersionString 2> /dev/null |" or die;
    $XcodeVersion = <VERSION>;
    close VERSION;
    chomp $XcodeVersion;
}

sub determineBaseProductDir
{
    return if defined $baseProductDir;
    open PRODUCT, "defaults read com.apple.Xcode PBXProductDirectory 2> /dev/null |" or die;
    $baseProductDir = <PRODUCT>;
    close PRODUCT;
    if ($baseProductDir) {
        chomp $baseProductDir;
        @baseProductDirOption = ();
    } else {
        chdirWebKit();
        $baseProductDir = `pwd` . "/WebKitBuild";
        @baseProductDirOption = ("SYMROOT=$baseProductDir");
    }
    $baseProductDir =~ s|^~/|$ENV{HOME}/|;
}

sub determineConfiguration
{
    return if defined $configuration;
    determineBaseProductDir();
    open CONFIGURATION, "$baseProductDir/Configuration" or die;
    $configuration = <CONFIGURATION>;
    close CONFIGURATION;
    if ($configuration) {
        chomp $configuration;
    } else {
        $configuration = "Deployment";
    }
}

sub determineConfigurationProductDir
{
    determineConfiguration();
    determineXCodeVersion();
    if ($XcodeVersion eq "2.0") {
        $configurationProductDir = $baseProductDir;
    } else {
        $configurationProductDir = "$baseProductDir/$configuration";
    }
}

sub baseProductDir
{
    determineBaseProductDir();
    return $baseProductDir;
}

sub productDir
{
    determineConfigurationProductDir();
    return $configurationProductDir;
}

sub XcodeOptions
{
    determineBaseProductDir();
    determineConfiguration();
    return (@baseProductDirOption, "-buildstyle", $configuration);
}

sub passedConfiguration
{
    for my $opt (@ARGV) {
        return "Development" if $opt =~ /^--devel/i;
        return "Deployment" if $opt =~ /^--deploy/i;
    }
    return undef;
}

sub setConfiguration
{
    my $passed = passedConfiguration();
    $configuration = $passed if $passed;
}

# Check to see that all the frameworks are built.
sub checkFrameworks
{
    determineConfigurationProductDir();
    for my $framework ("JavaScriptCore", "WebCore", "WebKit") {
        my $path = "$configurationProductDir/$framework.framework/Versions/A/$framework";
        die "Can't find built framework at \"$path\".\n" unless -x $path;
    }
}

1;
