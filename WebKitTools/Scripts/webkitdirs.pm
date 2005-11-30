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
   @EXPORT      = qw(&chdirWebKit &baseProductDir &productDir &XcodeOptions &passedConfiguration &setConfiguration &safariPath &checkFrameworks);
   %EXPORT_TAGS = ( );
   @EXPORT_OK   = ();
}

our @EXPORT_OK;

my $baseProductDir;
my @baseProductDirOption;
my $configuration;
my $configurationProductDir;
my $sourceDir;

sub determineSourceDir
{
    return if $sourceDir;
    $sourceDir = $FindBin::Bin;
    if ($sourceDir !~ s|/[^/]+/[^/]+$||) {
        die "Could not find two levels above source directory using FindBin.\n";
    }
}

sub determineBaseProductDir
{
    return if defined $baseProductDir;
    determineSourceDir();
    open PRODUCT, "defaults read com.apple.Xcode PBXProductDirectory 2> /dev/null |" or die;
    $baseProductDir = <PRODUCT>;
    close PRODUCT;
    if ($baseProductDir) {
        chomp $baseProductDir;
        $baseProductDir =~ s|^\Q$(SRCROOT)/..\E$|$sourceDir|;
        $baseProductDir =~ s|^\Q$(SRCROOT)/../|$sourceDir/|;
        $baseProductDir =~ s|^~/|$ENV{HOME}/|;
        die "Can't handle Xcode product directory with a ~ in it.\n" if $baseProductDir =~ /~/;
        die "Can't handle Xcode product directory with a variable in it.\n" if $baseProductDir =~ /\$/;
        @baseProductDirOption = ();
    } else {
        $baseProductDir = "$sourceDir/WebKitBuild";
        @baseProductDirOption = ("SYMROOT=$baseProductDir");
    }
}

sub determineConfiguration
{
    return if defined $configuration;
    determineBaseProductDir();
    if (open CONFIGURATION, "$baseProductDir/Configuration") {
        $configuration = <CONFIGURATION>;
        close CONFIGURATION;
    }
    if ($configuration) {
        chomp $configuration;
    } else {
        $configuration = "Deployment";
    }
}

sub determineConfigurationProductDir
{
    return if defined $configurationProductDir;
    determineBaseProductDir();
    determineConfiguration();
    $configurationProductDir = "$baseProductDir/$configuration";
}

sub chdirWebKit
{
    determineSourceDir();
    chdir $sourceDir or die;
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
    return (@baseProductDirOption, "-configuration", $configuration);
}

my $passedConfiguration;
my $searchedForPassedConfiguration;
sub determinePassedConfiguration
{
    return if $searchedForPassedConfiguration;
    $searchedForPassedConfiguration = 1;
    for my $i (0 .. $#ARGV) {
        my $opt = $ARGV[$i];
        if ($opt =~ /^--debug$/i || $opt =~ /^--devel/i) {
            splice(@ARGV, $i, 1);
            $passedConfiguration = "Development";
            return;
        }
        if ($opt =~ /^--release$/i || $opt =~ /^--deploy/i) {
            splice(@ARGV, $i, 1);
            $passedConfiguration = "Deployment";
            return;
        }
    }
    $passedConfiguration = undef;
}

sub passedConfiguration
{
    determinePassedConfiguration();
    return $passedConfiguration;
}

sub setConfiguration
{
    determinePassedConfiguration();
    $configuration = $passedConfiguration if $passedConfiguration;
}

# Locate Safari.
sub safariPath
{
    # Use WEBKIT_SAFARI environment variable if present.
    my $safariBundle = $ENV{WEBKIT_SAFARI};
    if (!$safariBundle) {
        determineConfigurationProductDir();
        # Use Safari.app in product directory if present (good for Safari development team).
        if (-d "$configurationProductDir/Safari.app") {
            $safariBundle = "$configurationProductDir/Safari.app";
        } else {
            # Otherwise use Safari.app in Applications directory.
            $safariBundle = "/Applications/Safari.app";
        }
    }
    my $safariPath = "$safariBundle/Contents/MacOS/Safari";
    die "Can't find executable at $safariPath.\n" unless -x $safariPath;
    return $safariPath;
}

sub builtDylibPathForName
{
    my $framework = shift;
    determineConfigurationProductDir();
    return "$configurationProductDir/$framework.framework/Versions/A/$framework";
}

# Check to see that all the frameworks are built.
sub checkFrameworks
{
    for my $framework ("JavaScriptCore", "WebCore", "WebKit") {
        my $path = builtDylibPathForName($framework);
        die "Can't find built framework at \"$path\".\n" unless -x $path;
    }
}

sub hasSVGSupport
{
    my $path = shift;
    my $frameworkSymbols = `nm $path`;
    my $hasSVGSupport = ($frameworkSymbols =~ /SVGElementImpl/);
    return $hasSVGSupport;
}

sub removeLibraryDependingOnSVG
{
    my $frameworkName = shift;
    my $shouldHaveSVG = shift;
    
    my $path = builtDylibPathForName($frameworkName);
    return unless -x $path;
    
    my $hasSVG = hasSVGSupport($path);
    system "rm -f $path" if ($shouldHaveSVG xor $hasSVG);
}

sub checkWebCoreSVGSupport
{
	my $required = shift;
    my $framework = "WebCore";
    my $path = builtDylibPathForName($framework);
    my $hasSVG = hasSVGSupport($path);
    if ($required && !$hasSVG) {
    	die "$framework at \"$path\" does not include SVG Support, please run build-webkit --svg\n";
    }
    return $hasSVG;
}

sub checkRequiredSystemConfig
{
    chomp(my $productVersion = `sw_vers -productVersion`);
    if ($productVersion lt "10.4") {
        print "*************************************************************\n";
        print "Mac OS X Version 10.4.0 or later is required to build WebKit.\n";
        print "You have " . $productVersion . ", thus the build will most likely fail.\n";
        print "*************************************************************\n";
    }
    my $xcodeVersion = `xcodebuild -version`;
    if ($xcodeVersion !~ /DevToolsCore-(\d+)/ || $1 < 620) {
        print "*************************************************************\n";
        print "Xcode Version 2.1 or later is required to build WebKit.\n";
        print "You have an earlier version of Xcode, thus the build will\n";
        print "most likely fail.  The latest Xcode is available from the web:\n";
        print "http://developer.apple.com/tools/xcode\n";
        print "*************************************************************\n";
    }
}

1;
