# Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
   @EXPORT      = qw(&chdirWebKit &baseProductDir &productDir &XcodeOptions &XcodeOptionString &XcodeOptionStringNoConfig &passedConfiguration &setConfiguration &safariPath &checkFrameworks &currentSVNRevision);
   %EXPORT_TAGS = ( );
   @EXPORT_OK   = ();
}

our @EXPORT_OK;

my $baseProductDir;
my @baseProductDirOption;
my $configuration;
my $configurationProductDir;
my $sourceDir;
my $currentSVNRevision;

# Variables for Win32 support
my $vcBuildPath;
my $windowsTmpPath;

sub determineSourceDir
{
    return if $sourceDir;
    $sourceDir = $FindBin::Bin;
    
    # walks up path checking each directory to see if it is the main WebKit project dir, 
    # defined by containing JavaScriptCore, WebCore, and WebKit
    until ((-d "$sourceDir/JavaScriptCore" && -d "$sourceDir/WebCore" && -d "$sourceDir/WebKit") || (-d "$sourceDir/Internal" && -d "$sourceDir/OpenSource"))
    {
        if ($sourceDir !~ s|/[^/]+$||) {
            die "Could not find top level webkit directory above source directory using FindBin.\n";
        }
    }

    $sourceDir = "$sourceDir/OpenSource" if -d "$sourceDir/OpenSource";
}

# used for scripts which are stored in a non-standard location
sub setSourceDir($)
{
    ($sourceDir) = @_;
}

sub determineBaseProductDir
{
    return if defined $baseProductDir;
    determineSourceDir();
    if (isOSX()) {
        open PRODUCT, "defaults read com.apple.Xcode PBXProductDirectory 2> /dev/null |" or die;
        $baseProductDir = <PRODUCT>;
        close PRODUCT;
        if ($baseProductDir) {
            chomp $baseProductDir;
            undef $baseProductDir unless $baseProductDir =~ /^\//;
        }
    } else {
        $baseProductDir = $ENV{"WEBKITOUTPUTDIR"};
        if (isCygwin() && $baseProductDir) {
            my $unixBuildPath = `cygpath --unix \"$baseProductDir\"`;
            chomp $unixBuildPath;
            $baseProductDir = $unixBuildPath;
        }
    }

    if ($baseProductDir && isOSX()) {
        $baseProductDir =~ s|^\Q$(SRCROOT)/..\E$|$sourceDir|;
        $baseProductDir =~ s|^\Q$(SRCROOT)/../|$sourceDir/|;
        $baseProductDir =~ s|^~/|$ENV{HOME}/|;
        die "Can't handle Xcode product directory with a ~ in it.\n" if $baseProductDir =~ /~/;
        die "Can't handle Xcode product directory with a variable in it.\n" if $baseProductDir =~ /\$/;
        @baseProductDirOption = ();
    }

    if (!defined($baseProductDir)) {
        $baseProductDir = "$sourceDir/WebKitBuild";
        @baseProductDirOption = ("SYMROOT=$baseProductDir") if (isOSX());
        if (isCygwin()) {
            my $dosBuildPath = `cygpath --windows \"$baseProductDir\"`;
            chomp $dosBuildPath;
            $ENV{"WEBKITOUTPUTDIR"} = $dosBuildPath;
        }
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
        # compatibility for people who have old Configuration files
        $configuration = "Release" if $configuration eq "Deployment";
        $configuration = "Debug" if $configuration eq "Development";
    } else {
        $configuration = "Release";
    }
}

sub determineConfigurationProductDir
{
    return if defined $configurationProductDir;
    determineBaseProductDir();
    if(isCygwin()) {
        $configurationProductDir = "$baseProductDir/bin";
    } else {
        determineConfiguration();
        $configurationProductDir = "$baseProductDir/$configuration";
    }
}

sub determineCurrentSVNRevision
{
    return if defined $currentSVNRevision;
    determineSourceDir();
    my $svnInfo = `svn info $sourceDir | grep Revision:`;
    ($currentSVNRevision) = ($svnInfo =~ m/Revision: (\d+).*/g);
    die "Unable to determine current SVN revision in $sourceDir" unless (defined $currentSVNRevision);
    return $currentSVNRevision;
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

sub sourceDir
{
    determineSourceDir();
    return $sourceDir;
}

sub productDir
{
    determineConfigurationProductDir();
    return $configurationProductDir;
}

sub configuration()
{
    determineConfiguration();
    return $configuration;
}

sub currentSVNRevision
{
    determineCurrentSVNRevision();
    return $currentSVNRevision;
}

sub XcodeOptions
{
    determineBaseProductDir();
    determineConfiguration();
    return (@baseProductDirOption, "-configuration", $configuration);
}

sub XcodeOptionString
{
    return join " ", XcodeOptions();
}

sub XcodeOptionStringNoConfig
{
    return join " ", @baseProductDirOption;
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
            $passedConfiguration = "Debug";
            return;
        }
        if ($opt =~ /^--release$/i || $opt =~ /^--deploy/i) {
            splice(@ARGV, $i, 1);
            $passedConfiguration = "Release";
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
    if (isQt()) {
        return "$configurationProductDir/$framework";
    }
    if (isOSX()) {
        return "$configurationProductDir/$framework.framework/Versions/A/$framework";
    }
    if (isCygwin()) {
        if ($framework eq "JavaScriptCore") {
                return "$baseProductDir/lib/$framework.lib";
        } else {
            return "$baseProductDir/$framework.intermediate/$configuration/$framework.intermediate/$framework.lib";
        }
    }

    die "Unsupported platform, can't determine built library locations.";
}

# Check to see that all the frameworks are built.
sub checkFrameworks
{
    my @frameworks = ("JavaScriptCore", "WebCore");
    push(@frameworks, "WebKit") if isOSX();
    for my $framework (@frameworks) {
        my $path = builtDylibPathForName($framework);
        die "Can't find built framework at \"$path\".\n" unless -x $path;
    }
}

sub hasSVGSupport
{
    return 0 if isCygwin();

    my $path = shift;

    if ((isQt()) and ($path =~ /WebCore/)) {
        $path .= "/../lib/libWebCore.so";
    }

    open NM, "-|", "nm", $path or die;
    my $hasSVGSupport = 0;
    while (<NM>) {
        $hasSVGSupport = 1 if /SVGElement/;
    }
    close NM;
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

sub isQt()
{
    return defined($ENV{'QTDIR'})
}

sub isQtWithQMake()
{
    our @qmakeBuild;
    return (isQt() and (@qmakeBuild eq 1))
}

sub isCygwin()
{
    return ($^O eq "cygwin");
}

sub isOSX()
{
    return ($^O eq "darwin");
}

sub checkRequiredSystemConfig
{
    if (isOSX()) {
        chomp(my $productVersion = `sw_vers -productVersion`);
        if ($productVersion lt "10.4") {
            print "*************************************************************\n";
            print "Mac OS X Version 10.4.0 or later is required to build WebKit.\n";
            print "You have " . $productVersion . ", thus the build will most likely fail.\n";
            print "*************************************************************\n";
        }
        my $xcodeVersion = `xcodebuild -version`;
        if ($xcodeVersion !~ /DevToolsCore-(\d+)/ || $1 < 747) {
            print "*************************************************************\n";
            print "Xcode Version 2.3 or later is required to build WebKit.\n";
            print "You have an earlier version of Xcode, thus the build will\n";
            print "most likely fail.  The latest Xcode is available from the web:\n";
            print "http://developer.apple.com/tools/xcode\n";
            print "*************************************************************\n";
        }
    }
    # Win32 and other platforms may want to check for minimum config
}

sub setupCygwinEnv()
{
    return if !isCygwin();
    return if $vcBuildPath;

    my $programFilesPath = `cygpath "$ENV{'PROGRAMFILES'}"`;
    chomp $programFilesPath;
    $vcBuildPath = "$programFilesPath/Microsoft Visual Studio 8/Common7/IDE/devenv.com";
    if (! -e $vcBuildPath) {
        # VC++ not found, try VC++ Express
        my $vsInstallDir;
        if ($ENV{'VSINSTALLDIR'}) {
            $vsInstallDir = $ENV{'VSINSTALLDIR'};
        } else {
            $programFilesPath = $ENV{'PROGRAMFILES'} || "C:\\Program Files";
            $vsInstallDir = "$programFilesPath/Microsoft Visual Studio 8";
        }
        $vsInstallDir = `cygpath "$vsInstallDir"`;
        chomp $vsInstallDir;
        $vcBuildPath = "$vsInstallDir/Common7/IDE/VCExpress.exe";
        if (! -e $vcBuildPath) {
            print "*************************************************************\n";
            print "Cannot find '$vcBuildPath'\n";
            print "Please execute the file 'vcvars32.bat' from\n";
            print "'$programFilesPath\\Microsoft Visual Studio 8\\VC\\bin\\'\n";
            print "to setup the necessary environment variables.\n";
            print "*************************************************************\n";
            die;
        }
    }

    $windowsTmpPath = `cygpath -w /tmp`;
    chomp $windowsTmpPath;
    print "Building results into: ", baseProductDir(), "\n";
    print "WEBKITOUTPUTDIR is set to: ", $ENV{"WEBKITOUTPUTDIR"}, "\n";
}

sub buildVisualStudioProject($)
{
    my ($project) = @_;
    setupCygwinEnv();

    chdir "$project.vcproj" or die "Failed to cd into $project.vcproj\n";
    my $config = configuration();

    print "$vcBuildPath $project.sln /build $config";
    my $result = system $vcBuildPath, "$project.sln", "/build", $config;
    chdir ".." or die;
    return $result;
}

sub buildCMakeProject($$)
{
    my ($project, $colorize) = @_;

    if ($project ne "WebKit") { 
        die "Qt/Linux builds JavaScriptCore/WebCore/WebKitQt in one shot! Only call it for 'WebKit'.\n";
    }

    my $config = configuration();
    my $prefix = $ENV{"WebKitInstallationPrefix"};
 
    my @buildArgs = ("-DCMAKE_BUILD_TYPE=$config");
    push @buildArgs, "-DCMAKE_INSTALL_PREFIX=" . $prefix if(defined($prefix));
    push @buildArgs, "-DWEBKIT_DO_NOT_USE_COLORFUL_OUTPUT=" . ($colorize ? "OFF" : "ON");
    push @buildArgs, "../../";

    print "Calling 'cmake @buildArgs' in " . baseProductDir() . "/$config ...\n\n";
    print "Installation directory: $prefix\n" if(defined($prefix));

    system "mkdir -p " . baseProductDir() . "/$config";
    chdir baseProductDir() . "/$config" or die "Failed to cd into " . baseProductDir() . "/$config \n";

    my $result = system "cmake", @buildArgs;
    if($result ne 0) {
       die "Failed to setup build environment using cmake!\n";
    }

    $result = system "make";
    chdir ".." or die;
    return $result;
}

sub buildQMakeProject($$)
{
    my ($project, $colorize) = @_;

    if ($project ne "WebKit") {
        die "Qt/Linux builds JavaScriptCore/WebCore/WebKitQt in one shot! Only call it for 'WebKit'.\n";
    }

    my $config = configuration();
    my $prefix = $ENV{"WebKitInstallationPrefix"};

    my @buildArgs = ("-r");
    push @buildArgs, "OUTPUT_DIR=" . baseProductDir() . "/$config";
    push @buildArgs, "../../WebKit.pro";

    print "Calling 'qmake @buildArgs' in " . baseProductDir() . "/$config ...\n\n";
    print "Installation directory: $prefix\n" if(defined($prefix));

    system "mkdir -p " . baseProductDir() . "/$config";
    chdir baseProductDir() . "/$config" or die "Failed to cd into " . baseProductDir() . "/$config \n";

    my $result = system "qmake", @buildArgs;
    if($result ne 0) {
       die "Failed to setup build environment using qmake!\n";
    }

    $result = system "make";
    chdir ".." or die;
    return $result;
}

1;
