# Copyright (C) 2005-2007, 2010-2016 Apple Inc. All rights reserved.
# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2011 Research In Motion Limited. All rights reserved.
# Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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

# Module to share code to get to WebKit directories.

use strict;
use version;
use warnings;
use Config;
use Cwd qw(realpath);
use Digest::MD5 qw(md5_hex);
use FindBin;
use File::Basename;
use File::Find;
use File::Path qw(make_path mkpath rmtree);
use File::Spec;
use File::stat;
use List::Util;
use POSIX;
use Time::HiRes qw(usleep);
use VCSUtils;

BEGIN {
   use Exporter   ();
   our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
   $VERSION     = 1.00;
   @ISA         = qw(Exporter);
   @EXPORT      = qw(
       &XcodeCoverageSupportOptions
       &XcodeOptionString
       &XcodeOptionStringNoConfig
       &XcodeOptions
       &XcodeStaticAnalyzerOption
       &appDisplayNameFromBundle
       &appendToEnvironmentVariableList
       &baseProductDir
       &chdirWebKit
       &checkFrameworks
       &cmakeBasedPortArguments
       &currentSVNRevision
       &debugSafari
       &executableProductDir
       &findOrCreateSimulatorForIOSDevice
       &iosSimulatorDeviceByName
       &nmPath
       &passedConfiguration
       &prependToEnvironmentVariableList
       &printHelpAndExitForRunAndDebugWebKitAppIfNeeded
       &productDir
       &quitIOSSimulator
       &relaunchIOSSimulator
       &restartIOSSimulatorDevice
       &runIOSWebKitApp
       &runMacWebKitApp
       &safariPath
       &iosVersion
       &setConfiguration
       &setupMacWebKitEnvironment
       &sharedCommandLineOptions
       &sharedCommandLineOptionsUsage
       &shutDownIOSSimulatorDevice
       &willUseIOSDeviceSDK
       &willUseIOSSimulatorSDK
       SIMULATOR_DEVICE_SUFFIX_FOR_WEBKIT_DEVELOPMENT
       USE_OPEN_COMMAND
   );
   %EXPORT_TAGS = ( );
   @EXPORT_OK   = ();
}

# Ports
use constant {
    AppleWin => "AppleWin",
    GTK      => "GTK",
    Efl      => "Efl",
    iOS      => "iOS",
    Mac      => "Mac",
    WinCairo => "WinCairo",
    Unknown  => "Unknown"
};

use constant USE_OPEN_COMMAND => 1; # Used in runMacWebKitApp().
use constant INCLUDE_OPTIONS_FOR_DEBUGGING => 1;
use constant SIMULATOR_DEVICE_STATE_SHUTDOWN => "1";
use constant SIMULATOR_DEVICE_STATE_BOOTED => "3";
use constant SIMULATOR_DEVICE_SUFFIX_FOR_WEBKIT_DEVELOPMENT  => "For WebKit Development";

# See table "Certificate types and names" on <https://developer.apple.com/library/ios/documentation/IDEs/Conceptual/AppDistributionGuide/MaintainingCertificates/MaintainingCertificates.html#//apple_ref/doc/uid/TP40012582-CH31-SW41>.
use constant IOS_DEVELOPMENT_CERTIFICATE_NAME_PREFIX => "iPhone Developer: ";

our @EXPORT_OK;

my $architecture;
my $asanIsEnabled;
my $numberOfCPUs;
my $maxCPULoad;
my $baseProductDir;
my @baseProductDirOption;
my $configuration;
my $xcodeSDK;
my $configurationForVisualStudio;
my $configurationProductDir;
my $sourceDir;
my $currentSVNRevision;
my $debugger;
my $didLoadIPhoneSimulatorNotification;
my $nmPath;
my $osXVersion;
my $iosVersion;
my $generateDsym;
my $isCMakeBuild;
my $isWin64;
my $isInspectorFrontend;
my $portName;
my $shouldUseGuardMalloc;
my $shouldNotUseNinja;
my $xcodeVersion;

my $unknownPortProhibited = 0;

# Variables for Win32 support
my $programFilesPath;
my $vcBuildPath;
my $vsInstallDir;
my $msBuildInstallDir;
my $vsVersion;
my $windowsSourceDir;
my $winVersion;
my $willUseVCExpressWhenBuilding = 0;

# Defined in VCSUtils.
sub exitStatus($);

sub findMatchingArguments($$);
sub hasArgument($$);

sub determineSourceDir
{
    return if $sourceDir;
    $sourceDir = $FindBin::Bin;
    $sourceDir =~ s|/+$||; # Remove trailing '/' as we would die later

    # walks up path checking each directory to see if it is the main WebKit project dir, 
    # defined by containing Sources, WebCore, and WebKit
    until ((-d File::Spec->catdir($sourceDir, "Source") && -d File::Spec->catdir($sourceDir, "Source", "WebCore") && -d File::Spec->catdir($sourceDir, "Source", "WebKit")) || (-d File::Spec->catdir($sourceDir, "Internal") && -d File::Spec->catdir($sourceDir, "OpenSource")))
    {
        if ($sourceDir !~ s|/[^/]+$||) {
            die "Could not find top level webkit directory above source directory using FindBin.\n";
        }
    }

    $sourceDir = File::Spec->catdir($sourceDir, "OpenSource") if -d File::Spec->catdir($sourceDir, "OpenSource");
}

sub currentPerlPath()
{
    my $thisPerl = $^X;
    if ($^O ne 'VMS') {
        $thisPerl .= $Config{_exe} unless $thisPerl =~ m/$Config{_exe}$/i;
    }
    return $thisPerl;
}

# used for scripts which are stored in a non-standard location
sub setSourceDir($)
{
    ($sourceDir) = @_;
}

sub determineNinjaVersion
{
    chomp(my $ninjaVersion = `ninja --version`);
    return $ninjaVersion;
}

sub determineXcodeVersion
{
    return if defined $xcodeVersion;
    my $xcodebuildVersionOutput = `xcodebuild -version`;
    $xcodeVersion = ($xcodebuildVersionOutput =~ /Xcode ([0-9](\.[0-9]+)*)/) ? $1 : "3.0";
}

sub readXcodeUserDefault($)
{
    my ($unprefixedKey) = @_;

    determineXcodeVersion();

    my $xcodeDefaultsDomain = (eval "v$xcodeVersion" lt v4) ? "com.apple.Xcode" : "com.apple.dt.Xcode";
    my $xcodeDefaultsPrefix = (eval "v$xcodeVersion" lt v4) ? "PBX" : "IDE";
    my $devnull = File::Spec->devnull();

    my $value = `defaults read $xcodeDefaultsDomain ${xcodeDefaultsPrefix}${unprefixedKey} 2> ${devnull}`;
    return if $?;

    chomp $value;
    return $value;
}

sub determineBaseProductDir
{
    return if defined $baseProductDir;
    determineSourceDir();

    my $setSharedPrecompsDir;
    $baseProductDir = $ENV{"WEBKIT_OUTPUTDIR"};

    if (!defined($baseProductDir) and isAppleMacWebKit()) {
        # Silently remove ~/Library/Preferences/xcodebuild.plist which can
        # cause build failure. The presence of
        # ~/Library/Preferences/xcodebuild.plist can prevent xcodebuild from
        # respecting global settings such as a custom build products directory
        # (<rdar://problem/5585899>).
        my $personalPlistFile = $ENV{HOME} . "/Library/Preferences/xcodebuild.plist";
        if (-e $personalPlistFile) {
            unlink($personalPlistFile) || die "Could not delete $personalPlistFile: $!";
        }

        determineXcodeVersion();

        if (eval "v$xcodeVersion" ge v4) {
            my $buildLocationStyle = join '', readXcodeUserDefault("BuildLocationStyle");
            if ($buildLocationStyle eq "Custom") {
                my $buildLocationType = join '', readXcodeUserDefault("CustomBuildLocationType");
                # FIXME: Read CustomBuildIntermediatesPath and set OBJROOT accordingly.
                $baseProductDir = readXcodeUserDefault("CustomBuildProductsPath") if $buildLocationType eq "Absolute";
            }

            # DeterminedByTargets corresponds to a setting of "Legacy" in Xcode.
            # It is the only build location style for which SHARED_PRECOMPS_DIR is not
            # overridden when building from within Xcode.
            $setSharedPrecompsDir = 1 if $buildLocationStyle ne "DeterminedByTargets";
        }

        if (!defined($baseProductDir)) {
            $baseProductDir = join '', readXcodeUserDefault("ApplicationwideBuildSettings");
            $baseProductDir = $1 if $baseProductDir =~ /SYMROOT\s*=\s*\"(.*?)\";/s;
        }

        undef $baseProductDir unless $baseProductDir =~ /^\//;
    }

    if (!defined($baseProductDir)) { # Port-specific checks failed, use default
        $baseProductDir = File::Spec->catdir($sourceDir, "WebKitBuild");
    }

    if (isGit() && isGitBranchBuild()) {
        my $branch = gitBranch();
        $baseProductDir = "$baseProductDir/$branch";
    }

    if (isAppleMacWebKit()) {
        $baseProductDir =~ s|^\Q$(SRCROOT)/..\E$|$sourceDir|;
        $baseProductDir =~ s|^\Q$(SRCROOT)/../|$sourceDir/|;
        $baseProductDir =~ s|^~/|$ENV{HOME}/|;
        die "Can't handle Xcode product directory with a ~ in it.\n" if $baseProductDir =~ /~/;
        die "Can't handle Xcode product directory with a variable in it.\n" if $baseProductDir =~ /\$/;
        @baseProductDirOption = ("SYMROOT=$baseProductDir", "OBJROOT=$baseProductDir");
        push(@baseProductDirOption, "SHARED_PRECOMPS_DIR=${baseProductDir}/PrecompiledHeaders") if $setSharedPrecompsDir;
    }

    if (isCygwin()) {
        my $dosBuildPath = `cygpath --windows \"$baseProductDir\"`;
        chomp $dosBuildPath;
        $ENV{"WEBKIT_OUTPUTDIR"} = $dosBuildPath;
        my $unixBuildPath = `cygpath --unix \"$baseProductDir\"`;
        chomp $unixBuildPath;
        $baseProductDir = $dosBuildPath;
    }
}

sub setBaseProductDir($)
{
    ($baseProductDir) = @_;
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

sub determineArchitecture
{
    return if defined $architecture;
    # make sure $architecture is defined in all cases
    $architecture = "";

    determineBaseProductDir();
    determineXcodeSDK();

    if (isAppleMacWebKit()) {
        if (open ARCHITECTURE, "$baseProductDir/Architecture") {
            $architecture = <ARCHITECTURE>;
            close ARCHITECTURE;
        }
        if ($architecture) {
            chomp $architecture;
        } else {
            if (not defined $xcodeSDK or $xcodeSDK =~ /^(\/$|macosx)/) {
                my $supports64Bit = `sysctl -n hw.optional.x86_64`;
                chomp $supports64Bit;
                $architecture = 'x86_64' if $supports64Bit;
            } elsif ($xcodeSDK =~ /^iphonesimulator/) {
                $architecture = 'x86_64';
            } elsif ($xcodeSDK =~ /^iphoneos/) {
                $architecture = 'armv7';
            }
        }
    } elsif (isCMakeBuild()) {
        my $host_processor = "";
        if (open my $cmake_sysinfo, "cmake --system-information |") {
            while (<$cmake_sysinfo>) {
                next unless index($_, 'CMAKE_SYSTEM_PROCESSOR') == 0;
                if (/^CMAKE_SYSTEM_PROCESSOR \"([^"]+)\"/) {
                    $architecture = $1;
                    $architecture = 'x86_64' if $architecture eq 'amd64';
                    last;
                }
            }
            close $cmake_sysinfo;
        }
    }

    if (!isAnyWindows()) {
        if (!$architecture) {
            # Fall back to output of `arch', if it is present.
            $architecture = `arch`;
            chomp $architecture;
        }

        if (!$architecture) {
            # Fall back to output of `uname -m', if it is present.
            $architecture = `uname -m`;
            chomp $architecture;
        }
    }

    $architecture = 'x86_64' if ($architecture =~ /amd64/ && isBSD());
}

sub determineASanIsEnabled
{
    return if defined $asanIsEnabled;
    determineBaseProductDir();

    $asanIsEnabled = 0;
    my $asanConfigurationValue;

    if (open ASAN, "$baseProductDir/ASan") {
        $asanConfigurationValue = <ASAN>;
        close ASAN;
        chomp $asanConfigurationValue;
        $asanIsEnabled = 1 if $asanConfigurationValue eq "YES";
    }
}

sub determineNumberOfCPUs
{
    return if defined $numberOfCPUs;
    if (defined($ENV{NUMBER_OF_PROCESSORS})) {
        $numberOfCPUs = $ENV{NUMBER_OF_PROCESSORS};
    } elsif (isLinux()) {
        # First try the nproc utility, if it exists. If we get no
        # results fall back to just interpretting /proc directly.
        chomp($numberOfCPUs = `nproc --all 2> /dev/null`);
        if ($numberOfCPUs eq "") {
            $numberOfCPUs = (grep /processor/, `cat /proc/cpuinfo`);
        }
    } elsif (isAnyWindows()) {
        # Assumes cygwin
        $numberOfCPUs = `ls /proc/registry/HKEY_LOCAL_MACHINE/HARDWARE/DESCRIPTION/System/CentralProcessor | wc -w`;
    } elsif (isDarwin() || isBSD()) {
        chomp($numberOfCPUs = `sysctl -n hw.ncpu`);
    }
}

sub determineMaxCPULoad
{
    return if defined $maxCPULoad;
    if (defined($ENV{MAX_CPU_LOAD})) {
        $maxCPULoad = $ENV{MAX_CPU_LOAD};
    }
}

sub jscPath($)
{
    my ($productDir) = @_;
    my $jscName = "jsc";
    $jscName .= "_debug"  if configuration() eq "Debug_All";
    $jscName .= ".exe" if (isAnyWindows());
    return "$productDir/$jscName" if -e "$productDir/$jscName";
    return "$productDir/JavaScriptCore.framework/Resources/$jscName";
}

sub argumentsForConfiguration()
{
    determineConfiguration();
    determineArchitecture();
    determineXcodeSDK();

    my @args = ();
    # FIXME: Is it necessary to pass --debug, --release, --32-bit or --64-bit?
    # These are determined automatically from stored configuration.
    push(@args, '--debug') if ($configuration =~ "^Debug");
    push(@args, '--release') if ($configuration =~ "^Release");
    push(@args, '--device') if (defined $xcodeSDK && $xcodeSDK =~ /^iphoneos/);
    push(@args, '--ios-simulator') if (defined $xcodeSDK && $xcodeSDK =~ /^iphonesimulator/);
    push(@args, '--32-bit') if ($architecture ne "x86_64" and !isWin64());
    push(@args, '--64-bit') if (isWin64());
    push(@args, '--gtk') if isGtk();
    push(@args, '--efl') if isEfl();
    push(@args, '--wincairo') if isWinCairo();
    push(@args, '--inspector-frontend') if isInspectorFrontend();
    return @args;
}

sub determineXcodeSDK
{
    return if defined $xcodeSDK;
    my $sdk;
    if (checkForArgumentAndRemoveFromARGVGettingValue("--sdk", \$sdk)) {
        $xcodeSDK = $sdk;
    }
    if (checkForArgumentAndRemoveFromARGV("--device")) {
        my $hasInternalSDK = exitStatus(system("xcrun --sdk iphoneos.internal --show-sdk-version > /dev/null 2>&1")) == 0;
        $xcodeSDK ||= $hasInternalSDK ? "iphoneos.internal" : "iphoneos";
    }
    if (checkForArgumentAndRemoveFromARGV("--ios-simulator")) {
        $xcodeSDK ||= 'iphonesimulator';
    }
}

sub xcodeSDK
{
    determineXcodeSDK();
    return $xcodeSDK;
}

sub setXcodeSDK($)
{
    ($xcodeSDK) = @_;
}


sub xcodeSDKPlatformName()
{
    determineXcodeSDK();
    return "" if !defined $xcodeSDK;
    return "iphoneos" if $xcodeSDK =~ /iphoneos/i;
    return "iphonesimulator" if $xcodeSDK =~ /iphonesimulator/i;
    return "macosx" if $xcodeSDK =~ /macosx/i;
    die "Couldn't determine platform name from Xcode SDK";
}

sub XcodeSDKPath
{
    determineXcodeSDK();

    die "Can't find the SDK path because no Xcode SDK was specified" if not $xcodeSDK;

    my $sdkPath = `xcrun --sdk $xcodeSDK --show-sdk-path` if $xcodeSDK;
    die 'Failed to get SDK path from xcrun' if $?;
    chomp $sdkPath;

    return $sdkPath;
}

sub xcodeSDKVersion
{
    determineXcodeSDK();

    die "Can't find the SDK version because no Xcode SDK was specified" if !$xcodeSDK;

    chomp(my $sdkVersion = `xcrun --sdk $xcodeSDK --show-sdk-version`);
    die "Failed to get SDK version from xcrun" if exitStatus($?);

    return $sdkVersion;
}

sub programFilesPath
{
    return $programFilesPath if defined $programFilesPath;

    $programFilesPath = $ENV{'PROGRAMFILES(X86)'} || $ENV{'PROGRAMFILES'} || "C:\\Program Files";

    return $programFilesPath;
}

sub visualStudioInstallDir
{
    return $vsInstallDir if defined $vsInstallDir;

    if ($ENV{'VSINSTALLDIR'}) {
        $vsInstallDir = $ENV{'VSINSTALLDIR'};
        $vsInstallDir =~ s|[\\/]$||;
    } else {
        $vsInstallDir = File::Spec->catdir(programFilesPath(), "Microsoft Visual Studio 14.0");
    }
    chomp($vsInstallDir = `cygpath "$vsInstallDir"`) if isCygwin();

    print "Using Visual Studio: $vsInstallDir\n";
    return $vsInstallDir;
}

sub msBuildInstallDir
{
    return $msBuildInstallDir if defined $msBuildInstallDir;

    $msBuildInstallDir = File::Spec->catdir(programFilesPath(), "MSBuild", "14.0", "Bin");
   
    chomp($msBuildInstallDir = `cygpath "$msBuildInstallDir"`) if isCygwin();

    print "Using MSBuild: $msBuildInstallDir\n";
    return $msBuildInstallDir;
}

sub visualStudioVersion
{
    return $vsVersion if defined $vsVersion;

    my $installDir = visualStudioInstallDir();

    $vsVersion = ($installDir =~ /Microsoft Visual Studio ([0-9]+\.[0-9]*)/) ? $1 : "14";

    print "Using Visual Studio $vsVersion\n";
    return $vsVersion;
}

sub determineConfigurationForVisualStudio
{
    return if defined $configurationForVisualStudio;
    determineConfiguration();
    # FIXME: We should detect when Debug_All or Production has been chosen.
    $configurationForVisualStudio = "/p:Configuration=" . $configuration;
}

sub usesPerConfigurationBuildDirectory
{
    # [Gtk] We don't have Release/Debug configurations in straight
    # autotool builds (non build-webkit). In this case and if
    # WEBKIT_OUTPUTDIR exist, use that as our configuration dir. This will
    # allows us to run run-webkit-tests without using build-webkit.
    return ($ENV{"WEBKIT_OUTPUTDIR"} && isGtk()) || isAppleWinWebKit();
}

sub determineConfigurationProductDir
{
    return if defined $configurationProductDir;
    determineBaseProductDir();
    determineConfiguration();
    if (isAppleWinWebKit() || isWinCairo()) {
        $configurationProductDir = File::Spec->catdir($baseProductDir, $configuration);
    } else {
        if (usesPerConfigurationBuildDirectory()) {
            $configurationProductDir = "$baseProductDir";
        } else {
            $configurationProductDir = "$baseProductDir/$configuration";
            $configurationProductDir .= "-" . xcodeSDKPlatformName() if isIOSWebKit();
        }
    }
}

sub setConfigurationProductDir($)
{
    ($configurationProductDir) = @_;
}

sub determineCurrentSVNRevision
{
    # We always update the current SVN revision here, and leave the caching
    # to currentSVNRevision(), so that changes to the SVN revision while the
    # script is running can be picked up by calling this function again.
    determineSourceDir();
    $currentSVNRevision = svnRevisionForDirectory($sourceDir);
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

sub executableProductDir
{
    my $productDirectory = productDir();

    my $binaryDirectory;
    if (isEfl() || isGtk()) {
        $binaryDirectory = "bin";
    } elsif (isAnyWindows()) {
        $binaryDirectory = isWin64() ? "bin64" : "bin32";
    } else {
        return $productDirectory;
    }

    return File::Spec->catdir($productDirectory, $binaryDirectory);
}

sub jscProductDir
{
    return executableProductDir();
}

sub configuration()
{
    determineConfiguration();
    return $configuration;
}

sub asanIsEnabled()
{
    determineASanIsEnabled();
    return $asanIsEnabled;
}

sub configurationForVisualStudio()
{
    determineConfigurationForVisualStudio();
    return $configurationForVisualStudio;
}

sub currentSVNRevision
{
    determineCurrentSVNRevision() if not defined $currentSVNRevision;
    return $currentSVNRevision;
}

sub generateDsym()
{
    determineGenerateDsym();
    return $generateDsym;
}

sub determineGenerateDsym()
{
    return if defined($generateDsym);
    $generateDsym = checkForArgumentAndRemoveFromARGV("--dsym");
}

sub hasIOSDevelopmentCertificate()
{
    return !exitStatus(system("security find-identity -p codesigning | grep '" . IOS_DEVELOPMENT_CERTIFICATE_NAME_PREFIX . "' > /dev/null 2>&1"));
}

sub argumentsForXcode()
{
    my @args = ();
    push @args, "DEBUG_INFORMATION_FORMAT=dwarf-with-dsym" if generateDsym();
    return @args;
}

sub XcodeOptions
{
    determineBaseProductDir();
    determineConfiguration();
    determineArchitecture();
    determineASanIsEnabled();
    determineXcodeSDK();

    my @options;
    push @options, "-UseSanitizedBuildSystemEnvironment=YES";
    push @options, ("-configuration", $configuration);
    push @options, ("-xcconfig", sourceDir() . "/Tools/asan/asan.xcconfig", "ASAN_IGNORE=" . sourceDir() . "/Tools/asan/webkit-asan-ignore.txt") if $asanIsEnabled;
    push @options, @baseProductDirOption;
    push @options, "ARCHS=$architecture" if $architecture;
    push @options, "SDKROOT=$xcodeSDK" if $xcodeSDK;
    if (willUseIOSDeviceSDK()) {
        push @options, "ENABLE_BITCODE=NO";
        if (hasIOSDevelopmentCertificate()) {
            # FIXME: May match more than one installed development certificate.
            push @options, "CODE_SIGN_IDENTITY=" . IOS_DEVELOPMENT_CERTIFICATE_NAME_PREFIX;
        } else {
            push @options, "CODE_SIGN_IDENTITY="; # No identity
            push @options, "CODE_SIGNING_REQUIRED=NO";
        }
    }
    push @options, argumentsForXcode();
    return @options;
}

sub XcodeOptionString
{
    return join " ", XcodeOptions();
}

sub XcodeOptionStringNoConfig
{
    return join " ", @baseProductDirOption;
}

sub XcodeCoverageSupportOptions()
{
    my @coverageSupportOptions = ();
    push @coverageSupportOptions, "GCC_GENERATE_TEST_COVERAGE_FILES=YES";
    push @coverageSupportOptions, "GCC_INSTRUMENT_PROGRAM_FLOW_ARCS=YES";
    return @coverageSupportOptions;
}

sub XcodeStaticAnalyzerOption()
{
    return "RUN_CLANG_STATIC_ANALYZER=YES";
}

my $passedConfiguration;
my $searchedForPassedConfiguration;
sub determinePassedConfiguration
{
    return if $searchedForPassedConfiguration;
    $searchedForPassedConfiguration = 1;
    $passedConfiguration = undef;

    if (checkForArgumentAndRemoveFromARGV("--debug")) {
        $passedConfiguration = "Debug";
    } elsif(checkForArgumentAndRemoveFromARGV("--release")) {
        $passedConfiguration = "Release";
    } elsif (checkForArgumentAndRemoveFromARGV("--profile") || checkForArgumentAndRemoveFromARGV("--profiling")) {
        $passedConfiguration = "Profiling";
    }
}

sub passedConfiguration
{
    determinePassedConfiguration();
    return $passedConfiguration;
}

sub setConfiguration
{
    setArchitecture();

    if (my $config = shift @_) {
        $configuration = $config;
        return;
    }

    determinePassedConfiguration();
    $configuration = $passedConfiguration if $passedConfiguration;
}


my $passedArchitecture;
my $searchedForPassedArchitecture;
sub determinePassedArchitecture
{
    return if $searchedForPassedArchitecture;
    $searchedForPassedArchitecture = 1;

    $passedArchitecture = undef;
    if (checkForArgumentAndRemoveFromARGV("--32-bit")) {
        if (isAppleMacWebKit()) {
            # PLATFORM_IOS: Don't run `arch` command inside Simulator environment
            local %ENV = %ENV;
            delete $ENV{DYLD_ROOT_PATH};
            delete $ENV{DYLD_FRAMEWORK_PATH};

            $passedArchitecture = `arch`;
            chomp $passedArchitecture;
        }
    }
}

sub passedArchitecture
{
    determinePassedArchitecture();
    return $passedArchitecture;
}

sub architecture()
{
    determineArchitecture();
    return $architecture;
}

sub numberOfCPUs()
{
    determineNumberOfCPUs();
    return $numberOfCPUs;
}

sub maxCPULoad()
{
    determineMaxCPULoad();
    return $maxCPULoad;
}

sub setArchitecture
{
    if (my $arch = shift @_) {
        $architecture = $arch;
        return;
    }

    determinePassedArchitecture();
    $architecture = $passedArchitecture if $passedArchitecture;
}

sub skipSafariExecutableEntitlementChecks
{
    return `defaults read /Library/Preferences/org.webkit.BuildConfiguration SkipSafariExecutableEntitlementChecks 2>/dev/null` eq "1\n";
}

sub executableHasEntitlements
{
    my $executablePath = shift;
    return (`codesign -d --entitlements - $executablePath 2>&1` =~ /<key>/);
}

sub safariPathFromSafariBundle
{
    my ($safariBundle) = @_;

    die "Safari path is only relevant on Apple Mac platform\n" unless isAppleMacWebKit();

    my $safariPath = "$safariBundle/Contents/MacOS/Safari";
    return $safariPath if skipSafariExecutableEntitlementChecks();

    my $safariForWebKitDevelopmentPath = "$safariBundle/Contents/MacOS/SafariForWebKitDevelopment";
    return $safariForWebKitDevelopmentPath if -f $safariForWebKitDevelopmentPath && executableHasEntitlements($safariPath);

    return $safariPath;
}

sub installedSafariPath
{
    return safariPathFromSafariBundle("/Applications/Safari.app");
}

# Locate Safari.
sub safariPath
{
    die "Safari path is only relevant on Apple Mac platform\n" unless isAppleMacWebKit();

    # Use WEBKIT_SAFARI environment variable if present.
    my $safariBundle = $ENV{WEBKIT_SAFARI};
    if (!$safariBundle) {
        determineConfigurationProductDir();
        # Use Safari.app in product directory if present (good for Safari development team).
        if (-d "$configurationProductDir/Safari.app") {
            $safariBundle = "$configurationProductDir/Safari.app";
        }
        if (!$safariBundle) {
            return installedSafariPath();
        }
    }
    my $safariPath = safariPathFromSafariBundle($safariBundle);
    die "Can't find executable at $safariPath.\n" if !-x $safariPath;
    return $safariPath;
}

sub builtDylibPathForName
{
    my $libraryName = shift;
    determineConfigurationProductDir();

    if (isGtk()) {
        my $extension = isDarwin() ? ".dylib" : ".so";
        return "$configurationProductDir/lib/libwebkit2gtk-4.0" . $extension;
    }
    if (isEfl()) {
        return "$configurationProductDir/lib/libewebkit2.so";
    }
    if (isIOSWebKit()) {
        return "$configurationProductDir/$libraryName.framework/$libraryName";
    }
    if (isAppleMacWebKit()) {
        return "$configurationProductDir/$libraryName.framework/Versions/A/$libraryName";
    }
    if (isAppleWinWebKit()) {
        if ($libraryName eq "JavaScriptCore") {
            return "$baseProductDir/lib/$libraryName.lib";
        } else {
            return "$baseProductDir/$libraryName.intermediate/$configuration/$libraryName.intermediate/$libraryName.lib";
        }
    }

    die "Unsupported platform, can't determine built library locations.\nTry `build-webkit --help` for more information.\n";
}

# Check to see that all the frameworks are built.
sub checkFrameworks # FIXME: This is a poor name since only the Mac calls built WebCore a Framework.
{
    return if isAnyWindows();
    my @frameworks = ("JavaScriptCore", "WebCore");
    push(@frameworks, "WebKit") if isAppleMacWebKit(); # FIXME: This seems wrong, all ports should have a WebKit these days.
    for my $framework (@frameworks) {
        my $path = builtDylibPathForName($framework);
        die "Can't find built framework at \"$path\".\n" unless -e $path;
    }
}

sub isInspectorFrontend()
{
    determineIsInspectorFrontend();
    return $isInspectorFrontend;
}

sub determineIsInspectorFrontend()
{
    return if defined($isInspectorFrontend);
    $isInspectorFrontend = checkForArgumentAndRemoveFromARGV("--inspector-frontend");
}

sub commandExists($)
{
    my $command = shift;
    my $devnull = File::Spec->devnull();

    if (isAnyWindows()) {
        return exitStatus(system("where /q $command >$devnull 2>&1")) == 0;
    }
    return exitStatus(system("which $command >$devnull 2>&1")) == 0;
}

sub checkForArgumentAndRemoveFromARGV($)
{
    my $argToCheck = shift;
    return checkForArgumentAndRemoveFromArrayRef($argToCheck, \@ARGV);
}

sub checkForArgumentAndRemoveFromArrayRefGettingValue($$$)
{
    my ($argToCheck, $valueRef, $arrayRef) = @_;
    my $argumentStartRegEx = qr#^$argToCheck(?:=\S|$)#;
    my $i = 0;
    for (; $i < @$arrayRef; ++$i) {
        last if $arrayRef->[$i] =~ $argumentStartRegEx;
    }
    if ($i >= @$arrayRef) {
        return $$valueRef = undef;
    }
    my ($key, $value) = split("=", $arrayRef->[$i]);
    splice(@$arrayRef, $i, 1);
    if (defined($value)) {
        # e.g. --sdk=iphonesimulator
        return $$valueRef = $value;
    }
    return $$valueRef = splice(@$arrayRef, $i, 1); # e.g. --sdk iphonesimulator
}

sub checkForArgumentAndRemoveFromARGVGettingValue($$)
{
    my ($argToCheck, $valueRef) = @_;
    return checkForArgumentAndRemoveFromArrayRefGettingValue($argToCheck, $valueRef, \@ARGV);
}

sub findMatchingArguments($$)
{
    my ($argToCheck, $arrayRef) = @_;
    my @matchingIndices;
    foreach my $index (0 .. $#$arrayRef) {
        my $opt = $$arrayRef[$index];
        if ($opt =~ /^$argToCheck$/i ) {
            push(@matchingIndices, $index);
        }
    }
    return @matchingIndices; 
}

sub hasArgument($$)
{
    my ($argToCheck, $arrayRef) = @_;
    my @matchingIndices = findMatchingArguments($argToCheck, $arrayRef);
    return scalar @matchingIndices > 0;
}

sub checkForArgumentAndRemoveFromArrayRef
{
    my ($argToCheck, $arrayRef) = @_;
    my @indicesToRemove = findMatchingArguments($argToCheck, $arrayRef);
    my $removeOffset = 0;
    foreach my $index (@indicesToRemove) {
        splice(@$arrayRef, $index - $removeOffset++, 1);
    }
    return scalar @indicesToRemove > 0;
}

sub prohibitUnknownPort()
{
    $unknownPortProhibited = 1;
}

sub determinePortName()
{
    return if defined $portName;

    my %argToPortName = (
        efl => Efl,
        gtk => GTK,
        wincairo => WinCairo
    );

    for my $arg (sort keys %argToPortName) {
        if (checkForArgumentAndRemoveFromARGV("--$arg")) {
            die "Argument '--$arg' conflicts with selected port '$portName'\n"
                if defined $portName;

            $portName = $argToPortName{$arg};
        }
    }

    return if defined $portName;

    # Port was not selected via command line, use appropriate default value

    if (isAnyWindows()) {
        $portName = AppleWin;
    } elsif (isDarwin()) {
        determineXcodeSDK();
        if (willUseIOSDeviceSDK() || willUseIOSSimulatorSDK()) {
            $portName = iOS;
        } else {
            $portName = Mac;
        }
    } else {
        if ($unknownPortProhibited) {
            my $portsChoice = join "\n\t", qw(
                --efl
                --gtk
            );
            die "Please specify which WebKit port to build using one of the following options:"
                . "\n\t$portsChoice\n";
        }

        # If script is run without arguments we cannot determine port
        # TODO: This state should be outlawed
        $portName = Unknown;
    }
}

sub portName()
{
    determinePortName();
    return $portName;
}

sub isEfl()
{
    return portName() eq Efl;
}

sub isGtk()
{
    return portName() eq GTK;
}

# Determine if this is debian, ubuntu, linspire, or something similar.
sub isDebianBased()
{
    return -e "/etc/debian_version";
}

sub isFedoraBased()
{
    return -e "/etc/fedora-release";
}

sub isWinCairo()
{
    return portName() eq WinCairo;
}

sub isWin64()
{
    determineIsWin64();
    return $isWin64;
}

sub determineIsWin64()
{
    return if defined($isWin64);
    $isWin64 = checkForArgumentAndRemoveFromARGV("--64-bit");
}

sub determineIsWin64FromArchitecture($)
{
    my $arch = shift;
    $isWin64 = ($arch eq "x86_64");
    return $isWin64;
}

sub isCygwin()
{
    return ($^O eq "cygwin") || 0;
}

sub isAnyWindows()
{
    return isWindows() || isCygwin();
}

sub determineWinVersion()
{
    return if $winVersion;

    if (!isAnyWindows()) {
        $winVersion = -1;
        return;
    }

    my $versionString = `cmd /c ver`;
    $versionString =~ /(\d)\.(\d)\.(\d+)/;

    $winVersion = {
        major => $1,
        minor => $2,
        build => $3,
    };
}

sub winVersion()
{
    determineWinVersion();
    return $winVersion;
}

sub isWindows7SP0()
{
    return isAnyWindows() && winVersion()->{major} == 6 && winVersion()->{minor} == 1 && winVersion()->{build} == 7600;
}

sub isWindowsVista()
{
    return isAnyWindows() && winVersion()->{major} == 6 && winVersion()->{minor} == 0;
}

sub isWindowsXP()
{
    return isAnyWindows() && winVersion()->{major} == 5 && winVersion()->{minor} == 1;
}

sub isDarwin()
{
    return ($^O eq "darwin") || 0;
}

sub isWindows()
{
    return ($^O eq "MSWin32") || 0;
}

sub isLinux()
{
    return ($^O eq "linux") || 0;
}

sub isBSD()
{
    return ($^O eq "freebsd") || ($^O eq "openbsd") || ($^O eq "netbsd") || 0;
}

sub isARM()
{
    return ($Config{archname} =~ /^arm[v\-]/) || ($Config{archname} =~ /^aarch64[v\-]/);
}

sub isX86_64()
{
    return (architecture() eq "x86_64") || 0;
}

sub isCrossCompilation()
{
  my $compiler = "";
  $compiler = $ENV{'CC'} if (defined($ENV{'CC'}));
  if ($compiler =~ /gcc/) {
      my $compiler_options = `$compiler -v 2>&1`;
      my @host = $compiler_options =~ m/--host=(.*?)\s/;
      my @target = $compiler_options =~ m/--target=(.*?)\s/;

      return ($host[0] ne "" && $target[0] ne "" && $host[0] ne $target[0]);
  }
  return 0;
}

sub isAppleWebKit()
{
    return isAppleMacWebKit() || isAppleWinWebKit();
}

sub isAppleMacWebKit()
{
    return (portName() eq Mac) || isIOSWebKit();
}

sub isAppleWinWebKit()
{
    return portName() eq AppleWin;
}

sub iOSSimulatorDevicesPath
{
    return "$ENV{HOME}/Library/Developer/CoreSimulator/Devices";
}

sub iOSSimulatorDevices
{
    eval "require Foundation";
    my $devicesPath = iOSSimulatorDevicesPath();
    opendir(DEVICES, $devicesPath);
    my @udids = grep {
        $_ =~ m/[0-9A-F]{8}-([0-9A-F]{4}-){3}[0-9A-F]{12}/;
    } readdir(DEVICES);
    close(DEVICES);

    # FIXME: We should parse the device.plist file ourself and map the dictionary keys in it to known
    #        dictionary keys so as to decouple our representation of the plist from the actual structure
    #        of the plist, which may change.
    my @devices = map {
        Foundation::perlRefFromObjectRef(NSDictionary->dictionaryWithContentsOfFile_("$devicesPath/$_/device.plist"));
    } @udids;

    return @devices;
}

sub createiOSSimulatorDevice
{
    my $name = shift;
    my $deviceTypeId = shift;
    my $runtimeId = shift;

    my $created = system("xcrun", "--sdk", "iphonesimulator", "simctl", "create", $name, $deviceTypeId, $runtimeId) == 0;
    die "Couldn't create simulator device: $name $deviceTypeId $runtimeId" if not $created;

    system("xcrun", "--sdk", "iphonesimulator", "simctl", "list");

    print "Waiting for device to be created ...\n";
    sleep 5;
    for (my $tries = 0; $tries < 5; $tries++){
        my @devices = iOSSimulatorDevices();
        foreach my $device (@devices) {
            return $device if $device->{name} eq $name and $device->{deviceType} eq $deviceTypeId and $device->{runtime} eq $runtimeId;
        }
        sleep 5;
    }
    die "Device $name $deviceTypeId $runtimeId wasn't found in " . iOSSimulatorDevicesPath();
}

sub willUseIOSDeviceSDK()
{
    return xcodeSDKPlatformName() eq "iphoneos";
}

sub willUseIOSSimulatorSDK()
{
    return xcodeSDKPlatformName() eq "iphonesimulator";
}

sub isIOSWebKit()
{
    return portName() eq iOS;
}

sub determineNmPath()
{
    return if $nmPath;

    if (isAppleMacWebKit()) {
        $nmPath = `xcrun -find nm`;
        chomp $nmPath;
    }
    $nmPath = "nm" if !$nmPath;
}

sub nmPath()
{
    determineNmPath();
    return $nmPath;
}

sub splitVersionString
{
    my $versionString = shift;
    my @splitVersion = split(/\./, $versionString);
    @splitVersion >= 2 or die "Invalid version $versionString";
    $osXVersion = {
            "major" => $splitVersion[0],
            "minor" => $splitVersion[1],
            "subminor" => (defined($splitVersion[2]) ? $splitVersion[2] : 0),
    };
}

sub determineOSXVersion()
{
    return if $osXVersion;

    if (!isDarwin()) {
        $osXVersion = -1;
        return;
    }

    my $versionString = `sw_vers -productVersion`;
    $osXVersion = splitVersionString($versionString);
}

sub osXVersion()
{
    determineOSXVersion();
    return $osXVersion;
}

sub determineIOSVersion()
{
    return if $iosVersion;

    if (!isIOSWebKit()) {
        $iosVersion = -1;
        return;
    }

    my $versionString = xcodeSDKVersion();
    $iosVersion = splitVersionString($versionString);
}

sub iosVersion()
{
    determineIOSVersion();
    return $iosVersion;
}

sub isWindowsNT()
{
    return $ENV{'OS'} eq 'Windows_NT';
}

sub debugger
{
    determineDebugger();
    return $debugger;
}

sub determineDebugger
{
    return if defined($debugger);

    determineXcodeVersion();
    if (eval "v$xcodeVersion" ge v4.5) {
        $debugger = "lldb";
    } else {
        $debugger = "gdb";
    }

    if (checkForArgumentAndRemoveFromARGV("--use-lldb")) {
        $debugger = "lldb";
    }

    if (checkForArgumentAndRemoveFromARGV("--use-gdb")) {
        $debugger = "gdb";
    }
}

sub appendToEnvironmentVariableList($$)
{
    my ($name, $value) = @_;

    if (defined($ENV{$name})) {
        $ENV{$name} .= $Config{path_sep} . $value;
    } else {
        $ENV{$name} = $value;
    }
}

sub prependToEnvironmentVariableList($$)
{
    my ($name, $value) = @_;

    if (defined($ENV{$name})) {
        $ENV{$name} = $value . $Config{path_sep} . $ENV{$name};
    } else {
        $ENV{$name} = $value;
    }
}

sub sharedCommandLineOptions()
{
    return (
        "g|guard-malloc" => \$shouldUseGuardMalloc,
    );
}

sub sharedCommandLineOptionsUsage
{
    my %opts = @_;

    my %switches = (
        '-g|--guard-malloc' => 'Use guardmalloc when running executable',
    );

    my $indent = " " x ($opts{indent} || 2);
    my $switchWidth = List::Util::max(int($opts{switchWidth}), List::Util::max(map { length($_) } keys %switches) + ($opts{brackets} ? 2 : 0));

    my $result = "Common switches:\n";

    for my $switch (keys %switches) {
        my $switchName = $opts{brackets} ? "[" . $switch . "]" : $switch;
        $result .= sprintf("%s%-" . $switchWidth . "s %s\n", $indent, $switchName, $switches{$switch});
    }

    return $result;
}

sub setUpGuardMallocIfNeeded
{
    if (!isDarwin()) {
        return;
    }

    if (!defined($shouldUseGuardMalloc)) {
        $shouldUseGuardMalloc = checkForArgumentAndRemoveFromARGV("-g") || checkForArgumentAndRemoveFromARGV("--guard-malloc");
    }

    if ($shouldUseGuardMalloc) {
        appendToEnvironmentVariableList("DYLD_INSERT_LIBRARIES", "/usr/lib/libgmalloc.dylib");
        appendToEnvironmentVariableList("__XPC_DYLD_INSERT_LIBRARIES", "/usr/lib/libgmalloc.dylib");
    }
}

sub relativeScriptsDir()
{
    my $scriptDir = File::Spec->catpath("", File::Spec->abs2rel($FindBin::Bin, getcwd()), "");
    if ($scriptDir eq "") {
        $scriptDir = ".";
    }
    return $scriptDir;
}

sub launcherPath()
{
    my $relativeScriptsPath = relativeScriptsDir();
    if (isGtk() || isEfl()) {
        return "$relativeScriptsPath/run-minibrowser";
    } elsif (isAppleWebKit()) {
        return "$relativeScriptsPath/run-safari";
    }
}

sub launcherName()
{
    if (isGtk() || isEfl()) {
        return "MiniBrowser";
    } elsif (isAppleMacWebKit()) {
        return "Safari";
    } elsif (isAppleWinWebKit()) {
        return "MiniBrowser";
    }
}

sub checkRequiredSystemConfig
{
    if (isDarwin()) {
        chomp(my $productVersion = `sw_vers -productVersion`);
        if (eval "v$productVersion" lt v10.7.5) {
            print "*************************************************************\n";
            print "Mac OS X Version 10.7.5 or later is required to build WebKit.\n";
            print "You have " . $productVersion . ", thus the build will most likely fail.\n";
            print "*************************************************************\n";
        }
        my $xcodebuildVersionOutput = `xcodebuild -version`;
        my $xcodeVersion = ($xcodebuildVersionOutput =~ /Xcode ([0-9](\.[0-9]+)*)/) ? $1 : undef;
        if (!$xcodeVersion || $xcodeVersion && eval "v$xcodeVersion" lt v4.6) {
            print "*************************************************************\n";
            print "Xcode Version 4.6 or later is required to build WebKit.\n";
            print "You have an earlier version of Xcode, thus the build will\n";
            print "most likely fail. The latest Xcode is available from the App Store.\n";
            print "*************************************************************\n";
        }
    }
}

sub determineWindowsSourceDir()
{
    return if $windowsSourceDir;
    $windowsSourceDir = sourceDir();
    chomp($windowsSourceDir = `cygpath -w '$windowsSourceDir'`) if isCygwin();
}

sub windowsSourceDir()
{
    determineWindowsSourceDir();
    return $windowsSourceDir;
}

sub windowsSourceSourceDir()
{
    return File::Spec->catdir(windowsSourceDir(), "Source");
}

sub windowsLibrariesDir()
{
    return File::Spec->catdir(windowsSourceDir(), "WebKitLibraries", "win");
}

sub windowsOutputDir()
{
    return File::Spec->catdir(windowsSourceDir(), "WebKitBuild");
}

sub fontExists($)
{
    my $font = shift;
    my $cmd = "reg query \"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts\\" . $font ."\" 2>&1";
    my $val = `$cmd`;
    return $? == 0;
}

sub checkInstalledTools()
{
    # environment variables. Avoid until this is corrected.
    my $pythonVer = `python --version 2>&1`;
    die "You must have Python installed to build WebKit.\n" if ($?);

    # cURL 7.34.0 has a bug that prevents authentication with opensource.apple.com (and other things using SSL3).
    my $curlVer = `curl --version 2> NUL`;
    if (!$? and $curlVer =~ "(.*curl.*)") {
        $curlVer = $1;
        if ($curlVer =~ /libcurl\/7\.34\.0/) {
            print "cURL version 7.34.0 has a bug that prevents authentication with SSL v2 or v3.\n";
            print "cURL 7.33.0 is known to work. The cURL projects is preparing an update to\n";
            print "correct this problem.\n\n";
            die "Please install a working cURL and try again.\n";
        }
    }

    # MathML requires fonts that do not ship with Windows (at least through Windows 8). Warn the user if they are missing
    my @fonts = qw(STIXGeneral-Regular MathJax_Main-Regular);
    my @missing = ();
    foreach my $font (@fonts) {
        push @missing, $font if not fontExists($font);
    }

    if (scalar @missing > 0) {
        print "*************************************************************\n";
        print "Mathematical fonts, such as STIX and MathJax, are needed to\n";
        print "use the MathML feature.  You do not appear to have these fonts\n";
        print "on your system.\n\n";
        print "You can download a suitable set of fonts from the following URL:\n";
        print "https://developer.mozilla.org/Mozilla/MathML_Projects/Fonts\n";
        print "*************************************************************\n";
    }

    print "Installed tools are correct for the WebKit build.\n";
}

sub setupAppleWinEnv()
{
    return unless isAppleWinWebKit();

    checkInstalledTools();

    if (isWindowsNT()) {
        my $restartNeeded = 0;
        my %variablesToSet = ();

        # FIXME: We should remove this explicit version check for cygwin once we stop supporting Cygwin 1.7.9 or older versions. 
        # https://bugs.webkit.org/show_bug.cgi?id=85791
        my $uname_version = (POSIX::uname())[2];
        $uname_version =~ s/\(.*\)//;  # Remove the trailing cygwin version, if any.
        $uname_version =~ s/\-.*$//; # Remove trailing dash-version content, if any
        if (version->parse($uname_version) < version->parse("1.7.10")) {
            # Setting the environment variable 'CYGWIN' to 'tty' makes cygwin enable extra support (i.e., termios)
            # for UNIX-like ttys in the Windows console
            $variablesToSet{CYGWIN} = "tty" unless $ENV{CYGWIN};
        }
        
        # Those environment variables must be set to be able to build inside Visual Studio.
        $variablesToSet{WEBKIT_LIBRARIES} = windowsLibrariesDir() unless $ENV{WEBKIT_LIBRARIES};
        $variablesToSet{WEBKIT_OUTPUTDIR} = windowsOutputDir() unless $ENV{WEBKIT_OUTPUTDIR};
        $variablesToSet{MSBUILDDISABLENODEREUSE} = "1" unless $ENV{MSBUILDDISABLENODEREUSE};
        $variablesToSet{_IsNativeEnvironment} = "true" unless $ENV{_IsNativeEnvironment};
        $variablesToSet{PreferredToolArchitecture} = "x64" unless $ENV{PreferredToolArchitecture};

        foreach my $variable (keys %variablesToSet) {
            print "Setting the Environment Variable '" . $variable . "' to '" . $variablesToSet{$variable} . "'\n\n";
            my $ret = system "setx", $variable, $variablesToSet{$variable};
            if ($ret != 0) {
                system qw(regtool -s set), '\\HKEY_CURRENT_USER\\Environment\\' . $variable, $variablesToSet{$variable};
            }
            $restartNeeded ||=  $variable eq "WEBKIT_LIBRARIES" || $variable eq "WEBKIT_OUTPUTDIR";
        }

        if ($restartNeeded) {
            print "Please restart your computer before attempting to build inside Visual Studio.\n\n";
        }
    } else {
        if (!defined $ENV{'WEBKIT_LIBRARIES'} || !$ENV{'WEBKIT_LIBRARIES'}) {
            print "Warning: You must set the 'WebKit_Libraries' environment variable\n";
            print "         to be able build WebKit from within Visual Studio 2013 and newer.\n";
            print "         Make sure that 'WebKit_Libraries' points to the\n";
            print "         'WebKitLibraries/win' directory, not the 'WebKitLibraries/' directory.\n\n";
        }
        if (!defined $ENV{'WEBKIT_OUTPUTDIR'} || !$ENV{'WEBKIT_OUTPUTDIR'}) {
            print "Warning: You must set the 'WebKit_OutputDir' environment variable\n";
            print "         to be able build WebKit from within Visual Studio 2013 and newer.\n\n";
        }
        if (!defined $ENV{'MSBUILDDISABLENODEREUSE'} || !$ENV{'MSBUILDDISABLENODEREUSE'}) {
            print "Warning: You should set the 'MSBUILDDISABLENODEREUSE' environment variable to '1'\n";
            print "         to avoid periodic locked log files when building.\n\n";
        }
    }
    # FIXME (125180): Remove the following temporary 64-bit support once official support is available.
    if (isWin64() and !$ENV{'WEBKIT_64_SUPPORT'}) {
        print "Warning: You must set the 'WEBKIT_64_SUPPORT' environment variable\n";
        print "         to be able run WebKit or JavaScriptCore tests.\n\n";
    }
}

sub setupCygwinEnv()
{
    return if !isAnyWindows();
    return if $vcBuildPath;

    my $programFilesPath = programFilesPath();
    my $visualStudioPath = File::Spec->catfile(visualStudioInstallDir(), qw(Common7 IDE devenv.com));
    if (-e $visualStudioPath) {
        # Visual Studio is installed;
        if (visualStudioVersion() eq "12") {
            $visualStudioPath = File::Spec->catfile(visualStudioInstallDir(), qw(Common7 IDE devenv.exe));
        }
    } else {
        # Visual Studio not found, try VC++ Express
        $visualStudioPath = File::Spec->catfile(visualStudioInstallDir(), qw(Common7 IDE WDExpress.exe));
        if (! -e $visualStudioPath) {
            print "*************************************************************\n";
            print "Cannot find '$visualStudioPath'\n";
            print "Please execute the file 'vcvars32.bat' from\n";
            print "'$programFilesPath\\Microsoft Visual Studio 14.0\\VC\\bin\\'\n";
            print "to setup the necessary environment variables.\n";
            print "*************************************************************\n";
            die;
        }
        $willUseVCExpressWhenBuilding = 1;
    }

    print "Building results into: ", baseProductDir(), "\n";
    print "WEBKIT_OUTPUTDIR is set to: ", $ENV{"WEBKIT_OUTPUTDIR"}, "\n";
    print "WEBKIT_LIBRARIES is set to: ", $ENV{"WEBKIT_LIBRARIES"}, "\n";
    # FIXME (125180): Remove the following temporary 64-bit support once official support is available.
    print "WEBKIT_64_SUPPORT is set to: ", $ENV{"WEBKIT_64_SUPPORT"}, "\n" if isWin64();

    # We will actually use MSBuild to build WebKit, but we need to find the Visual Studio install (above) to make
    # sure we use the right options.
    $vcBuildPath = File::Spec->catfile(msBuildInstallDir(), qw(MSBuild.exe));
    if (! -e $vcBuildPath) {
        print "*************************************************************\n";
        print "Cannot find '$vcBuildPath'\n";
        print "Please make sure execute that the Microsoft .NET Framework SDK\n";
        print "is installed on this machine.\n";
        print "*************************************************************\n";
        die;
    }
}

sub dieIfWindowsPlatformSDKNotInstalled
{
    my $registry32Path = "/proc/registry/";
    my $registry64Path = "/proc/registry64/";
    my @windowsPlatformSDKRegistryEntries = (
        "HKEY_LOCAL_MACHINE/SOFTWARE/Microsoft/Microsoft SDKs/Windows/v8.0A",
        "HKEY_LOCAL_MACHINE/SOFTWARE/Microsoft/Microsoft SDKs/Windows/v8.0",
        "HKEY_LOCAL_MACHINE/SOFTWARE/Microsoft/Microsoft SDKs/Windows/v7.1A",
        "HKEY_LOCAL_MACHINE/SOFTWARE/Microsoft/Microsoft SDKs/Windows/v7.0A",
        "HKEY_LOCAL_MACHINE/SOFTWARE/Microsoft/MicrosoftSDK/InstalledSDKs/D2FF9F89-8AA2-4373-8A31-C838BF4DBBE1",
    );

    # FIXME: It would be better to detect whether we are using 32- or 64-bit Windows
    # and only check the appropriate entry. But for now we just blindly check both.
    my $recommendedPlatformSDK = $windowsPlatformSDKRegistryEntries[0];

    while (@windowsPlatformSDKRegistryEntries) {
        my $windowsPlatformSDKRegistryEntry = shift @windowsPlatformSDKRegistryEntries;
        return if (-e $registry32Path . $windowsPlatformSDKRegistryEntry) || (-e $registry64Path . $windowsPlatformSDKRegistryEntry);
    }

    print "*************************************************************\n";
    print "Cannot find registry entry '$recommendedPlatformSDK'.\n";
    print "Please download and install the Microsoft Windows SDK\n";
    print "from <http://www.microsoft.com/en-us/download/details.aspx?id=8279>.\n\n";
    print "Then follow step 2 in the Windows section of the \"Installing Developer\n";
    print "Tools\" instructions at <http://www.webkit.org/building/tools.html>.\n";
    print "*************************************************************\n";
    die;
}

sub buildXCodeProject($$@)
{
    my ($project, $clean, @extraOptions) = @_;

    if ($clean) {
        push(@extraOptions, "-alltargets");
        push(@extraOptions, "clean");
    }

    push(@extraOptions, ("-sdk", xcodeSDK())) if isIOSWebKit();

    chomp($ENV{DSYMUTIL_NUM_THREADS} = `sysctl -n hw.activecpu`);
    return system "xcodebuild", "-project", "$project.xcodeproj", @extraOptions;
}

sub usingVisualStudioExpress()
{
    setupCygwinEnv();
    return $willUseVCExpressWhenBuilding;
}

sub buildVisualStudioProject
{
    my ($project, $clean) = @_;
    setupCygwinEnv();

    my $config = configurationForVisualStudio();

    dieIfWindowsPlatformSDKNotInstalled() if $willUseVCExpressWhenBuilding;

    chomp($project = `cygpath -w "$project"`) if isCygwin();

    my $action = "/t:build";
    if ($clean) {
        $action = "/t:clean";
    }

    my $platform = "/p:Platform=" . (isWin64() ? "x64" : "Win32");
    my $logPath = File::Spec->catdir($baseProductDir, $configuration);
    make_path($logPath) unless -d $logPath or $logPath eq ".";

    my $errorLogFile = File::Spec->catfile($logPath, "webkit_errors.log");
    chomp($errorLogFile = `cygpath -w "$errorLogFile"`) if isCygwin();
    my $errorLogging = "/flp:LogFile=" . $errorLogFile . ";ErrorsOnly";

    my $warningLogFile = File::Spec->catfile($logPath, "webkit_warnings.log");
    chomp($warningLogFile = `cygpath -w "$warningLogFile"`) if isCygwin();
    my $warningLogging = "/flp1:LogFile=" . $warningLogFile . ";WarningsOnly";

    my @command = ($vcBuildPath, "/verbosity:minimal", $project, $action, $config, $platform, "/fl", $errorLogging, "/fl1", $warningLogging);
    print join(" ", @command), "\n";
    return system @command;
}

sub getJhbuildPath()
{
    my @jhbuildPath = File::Spec->splitdir(baseProductDir());
    if (isGit() && isGitBranchBuild() && gitBranch()) {
        pop(@jhbuildPath);
    }
    if (isEfl()) {
        push(@jhbuildPath, "DependenciesEFL");
    } elsif (isGtk()) {
        push(@jhbuildPath, "DependenciesGTK");
    } else {
        die "Cannot get JHBuild path for platform that isn't GTK+ or EFL.\n";
    }
    return File::Spec->catdir(@jhbuildPath);
}

sub isCachedArgumentfileOutOfDate($@)
{
    my ($filename, $currentContents) = @_;

    if (! -e $filename) {
        return 1;
    }

    open(CONTENTS_FILE, $filename);
    chomp(my $previousContents = <CONTENTS_FILE>);
    close(CONTENTS_FILE);

    if ($previousContents ne $currentContents) {
        print "Contents for file $filename have changed.\n";
        print "Previous contents were: $previousContents\n\n";
        print "New contents are: $currentContents\n";
        return 1;
    }

    return 0;
}

sub wrapperPrefixIfNeeded()
{
    if (isAnyWindows()) {
        return ();
    }
    if (isAppleMacWebKit()) {
        return ("xcrun");
    }
    if (-e getJhbuildPath()) {
        my @prefix = (File::Spec->catfile(sourceDir(), "Tools", "jhbuild", "jhbuild-wrapper"));
        if (isEfl()) {
            push(@prefix, "--efl");
        } elsif (isGtk()) {
            push(@prefix, "--gtk");
        }
        push(@prefix, "run");

        return @prefix;
    }

    return ();
}

sub cmakeCachePath()
{
    return File::Spec->catdir(baseProductDir(), configuration(), "CMakeCache.txt");
}

sub shouldRemoveCMakeCache(@)
{
    my ($cacheFilePath, @buildArgs) = @_;

    # We check this first, because we always want to create this file for a fresh build.
    my $productDir = File::Spec->catdir(baseProductDir(), configuration());
    my $optionsCache = File::Spec->catdir($productDir, "build-webkit-options.txt");
    my $joinedBuildArgs = join(" ", @buildArgs);
    if (isCachedArgumentfileOutOfDate($optionsCache, $joinedBuildArgs)) {
        File::Path::mkpath($productDir) unless -d $productDir;
        open(CACHED_ARGUMENTS, ">", $optionsCache);
        print CACHED_ARGUMENTS $joinedBuildArgs;
        close(CACHED_ARGUMENTS);

        return 1;
    }

    my $cmakeCache = cmakeCachePath();
    unless (-e $cmakeCache) {
        return 0;
    }

    my $cacheFileModifiedTime = stat($cmakeCache)->mtime;
    my $platformConfiguration = File::Spec->catdir(sourceDir(), "Source", "cmake", "Options" . cmakeBasedPortName() . ".cmake");
    if ($cacheFileModifiedTime < stat($platformConfiguration)->mtime) {
        return 1;
    }

    my $globalConfiguration = File::Spec->catdir(sourceDir(), "Source", "cmake", "OptionsCommon.cmake");
    if ($cacheFileModifiedTime < stat($globalConfiguration)->mtime) {
        return 1;
    }

    my $inspectorUserInterfaceDircetory = File::Spec->catdir(sourceDir(), "Source", "WebInspectorUI", "UserInterface");
    if ($cacheFileModifiedTime < stat($inspectorUserInterfaceDircetory)->mtime) {
        return 1;
    }

    return 0;
}

sub removeCMakeCache(@)
{
    my (@buildArgs) = @_;
    if (shouldRemoveCMakeCache(@buildArgs)) {
        my $cmakeCache = cmakeCachePath();
        unlink($cmakeCache) if -e $cmakeCache;
    }
}

sub canUseNinja(@)
{
    if (!defined($shouldNotUseNinja)) {
        $shouldNotUseNinja = checkForArgumentAndRemoveFromARGV("--no-ninja");
    }

    if ($shouldNotUseNinja) {
        return 0;
    }

    # Test both ninja and ninja-build. Fedora uses ninja-build and has patched CMake to also call ninja-build.
    return commandExists("ninja") || commandExists("ninja-build");
}

sub canUseNinjaGenerator(@)
{
    # Check that a Ninja generator is installed
    my $devnull = File::Spec->devnull();
    return exitStatus(system("cmake -N -G Ninja >$devnull 2>&1")) == 0;
}

sub canUseEclipseNinjaGenerator(@)
{
    # Check that eclipse and eclipse Ninja generator is installed
    my $devnull = File::Spec->devnull();
    return commandExists("eclipse") && exitStatus(system("cmake -N -G 'Eclipse CDT4 - Ninja' >$devnull 2>&1")) == 0;
}

sub cmakeGeneratedBuildfile(@)
{
    my ($willUseNinja) = @_;
    if ($willUseNinja) {
        return File::Spec->catfile(baseProductDir(), configuration(), "build.ninja")
    } elsif (isAnyWindows()) {
        return File::Spec->catfile(baseProductDir(), configuration(), "WebKit.sln")
    } else {
        return File::Spec->catfile(baseProductDir(), configuration(), "Makefile")
    }
}

sub generateBuildSystemFromCMakeProject
{
    my ($prefixPath, @cmakeArgs) = @_;
    my $config = configuration();
    my $port = cmakeBasedPortName();
    my $buildPath = File::Spec->catdir(baseProductDir(), $config);
    File::Path::mkpath($buildPath) unless -d $buildPath;
    my $originalWorkingDirectory = getcwd();
    chdir($buildPath) or die;

    # We try to be smart about when to rerun cmake, so that we can have faster incremental builds.
    my $willUseNinja = canUseNinja() && canUseNinjaGenerator();
    if (-e cmakeCachePath() && -e cmakeGeneratedBuildfile($willUseNinja)) {
        return 0;
    }

    my @args;
    push @args, "-DPORT=\"$port\"";
    push @args, "-DCMAKE_INSTALL_PREFIX=\"$prefixPath\"" if $prefixPath;
    push @args, "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON" if isGtk();
    if ($config =~ /release/i) {
        push @args, "-DCMAKE_BUILD_TYPE=Release";
    } elsif ($config =~ /debug/i) {
        push @args, "-DCMAKE_BUILD_TYPE=Debug";
    }

    if ($willUseNinja) {
        push @args, "-G";
        if (canUseEclipseNinjaGenerator()) {
            push @args, "'Eclipse CDT4 - Ninja'";
        } else {
            push @args, "Ninja";
        }
    } elsif (isAnyWindows() && isWin64()) {
        push @args, '-G "Visual Studio 14 2015 Win64"';
    }

    # GTK+ has a production mode, but build-webkit should always use developer mode.
    push @args, "-DDEVELOPER_MODE=ON" if isEfl() || isGtk();

    # Don't warn variables which aren't used by cmake ports.
    push @args, "--no-warn-unused-cli";
    push @args, @cmakeArgs if @cmakeArgs;

    my $cmakeSourceDir = isCygwin() ? windowsSourceDir() : sourceDir();
    push @args, '"' . $cmakeSourceDir . '"';

    # Compiler options to keep floating point values consistent
    # between 32-bit and 64-bit architectures.
    determineArchitecture();
    if ($architecture ne "x86_64" && !isARM() && !isCrossCompilation() && !isAnyWindows()) {
        $ENV{'CXXFLAGS'} = "-march=pentium4 -msse2 -mfpmath=sse " . ($ENV{'CXXFLAGS'} || "");
    }

    # We call system("cmake @args") instead of system("cmake", @args) so that @args is
    # parsed for shell metacharacters.
    my $wrapper = join(" ", wrapperPrefixIfNeeded()) . " ";
    my $returnCode = system($wrapper . "cmake @args");

    chdir($originalWorkingDirectory);
    return $returnCode;
}

sub buildCMakeGeneratedProject($)
{
    my ($makeArgs) = @_;
    my $config = configuration();
    my $buildPath = File::Spec->catdir(baseProductDir(), $config);
    if (! -d $buildPath) {
        die "Must call generateBuildSystemFromCMakeProject() before building CMake project.";
    }

    my $command = "cmake";
    my @args = ("--build", $buildPath, "--config", $config);
    push @args, ("--", $makeArgs) if $makeArgs;

    # GTK can use a build script to preserve colors and pretty-printing.
    if (isGtk() && -e "$buildPath/build.sh") {
        chdir "$buildPath" or die;
        $command = "$buildPath/build.sh";
        @args = ($makeArgs);
    }

    if ($ENV{VERBOSE} && canUseNinja()) {
        push @args, "-v";
        push @args, "-d keeprsp" if (version->parse(determineNinjaVersion()) >= version->parse("1.4.0"));
    }

    # We call system("cmake @args") instead of system("cmake", @args) so that @args is
    # parsed for shell metacharacters. In particular, $makeArgs may contain such metacharacters.
    my $wrapper = join(" ", wrapperPrefixIfNeeded()) . " ";
    return system($wrapper . "$command @args");
}

sub cleanCMakeGeneratedProject()
{
    my $config = configuration();
    my $buildPath = File::Spec->catdir(baseProductDir(), $config);
    if (-d $buildPath) {
        return system("cmake", "--build", $buildPath, "--config", $config, "--target", "clean");
    }
    return 0;
}

sub buildCMakeProjectOrExit($$$@)
{
    my ($clean, $prefixPath, $makeArgs, @cmakeArgs) = @_;
    my $returnCode;

    exit(exitStatus(cleanCMakeGeneratedProject())) if $clean;

    if (isEfl() && checkForArgumentAndRemoveFromARGV("--update-efl")) {
        system("perl", "$sourceDir/Tools/Scripts/update-webkitefl-libs") == 0 or die $!;
    }

    if (isGtk() && checkForArgumentAndRemoveFromARGV("--update-gtk")) {
        system("perl", "$sourceDir/Tools/Scripts/update-webkitgtk-libs") == 0 or die $!;
    }

    $returnCode = exitStatus(generateBuildSystemFromCMakeProject($prefixPath, @cmakeArgs));
    exit($returnCode) if $returnCode;

    $returnCode = exitStatus(buildCMakeGeneratedProject($makeArgs));
    exit($returnCode) if $returnCode;
    return 0;
}

sub cmakeBasedPortArguments()
{
    return ();
}

sub cmakeBasedPortName()
{
    return ucfirst portName();
}

sub determineIsCMakeBuild()
{
    return if defined($isCMakeBuild);
    $isCMakeBuild = checkForArgumentAndRemoveFromARGV("--cmake");
}

sub isCMakeBuild()
{
    return 1 unless isAppleMacWebKit();
    determineIsCMakeBuild();
    return $isCMakeBuild;
}

sub promptUser
{
    my ($prompt, $default) = @_;
    my $defaultValue = $default ? "[$default]" : "";
    print "$prompt $defaultValue: ";
    chomp(my $input = <STDIN>);
    return $input ? $input : $default;
}

sub appleApplicationSupportPath
{
    open INSTALL_DIR, "</proc/registry/HKEY_LOCAL_MACHINE/SOFTWARE/Apple\ Inc./Apple\ Application\ Support/InstallDir";
    my $path = <INSTALL_DIR>;
    $path =~ s/[\r\n\x00].*//;
    close INSTALL_DIR;

    my $unixPath = `cygpath -u '$path'`;
    chomp $unixPath;
    return $unixPath;
}

sub setPathForRunningWebKitApp
{
    my ($env) = @_;

    if (isAnyWindows()) {
        my $productBinaryDir = executableProductDir();
        if (isAppleWinWebKit()) {
            $env->{PATH} = join(':', $productBinaryDir, appleApplicationSupportPath(), $env->{PATH} || "");
        } elsif (isWinCairo()) {
            my $winCairoBin = sourceDir() . "/WebKitLibraries/win/" . (isWin64() ? "bin64/" : "bin32/");
            my $gstreamerBin = isWin64() ? $ENV{"GSTREAMER_1_0_ROOT_X86_64"} . "bin" : $ENV{"GSTREAMER_1_0_ROOT_X86"} . "bin";
            $env->{PATH} = join(':', $productBinaryDir, $winCairoBin, $gstreamerBin, $env->{PATH} || "");
        }
    }
}

sub printHelpAndExitForRunAndDebugWebKitAppIfNeeded
{
    return unless checkForArgumentAndRemoveFromARGV("--help");

    my ($includeOptionsForDebugging) = @_;

    print STDERR <<EOF;
Usage: @{[basename($0)]} [options] [args ...]
  --help                            Show this help message
  --no-saved-state                  Launch the application without state restoration (OS X 10.7 and later)
  -g|--guard-malloc                 Enable Guard Malloc (OS X only)
EOF

    if ($includeOptionsForDebugging) {
        print STDERR <<EOF;
  --use-gdb                         Use GDB (this is the default when using Xcode 4.4 or earlier)
  --use-lldb                        Use LLDB (this is the default when using Xcode 4.5 or later)
EOF
    }

    exit(1);
}

sub argumentsForRunAndDebugMacWebKitApp()
{
    my @args = ();
    if (checkForArgumentAndRemoveFromARGV("--no-saved-state")) {
        push @args, ("-ApplePersistenceIgnoreStateQuietly", "YES");
        # FIXME: Don't set ApplePersistenceIgnoreState once all supported OS versions respect ApplePersistenceIgnoreStateQuietly (rdar://15032886).
        push @args, ("-ApplePersistenceIgnoreState", "YES");
    }
    unshift @args, @ARGV;

    return @args;
}

sub setupMacWebKitEnvironment($)
{
    my ($dyldFrameworkPath) = @_;

    $dyldFrameworkPath = File::Spec->rel2abs($dyldFrameworkPath);

    prependToEnvironmentVariableList("DYLD_FRAMEWORK_PATH", $dyldFrameworkPath);
    prependToEnvironmentVariableList("__XPC_DYLD_FRAMEWORK_PATH", $dyldFrameworkPath);
    $ENV{WEBKIT_UNSET_DYLD_FRAMEWORK_PATH} = "YES";

    setUpGuardMallocIfNeeded();
}

sub setupIOSWebKitEnvironment($)
{
    my ($dyldFrameworkPath) = @_;
    $dyldFrameworkPath = File::Spec->rel2abs($dyldFrameworkPath);

    prependToEnvironmentVariableList("DYLD_FRAMEWORK_PATH", $dyldFrameworkPath);
    prependToEnvironmentVariableList("DYLD_LIBRARY_PATH", $dyldFrameworkPath);

    setUpGuardMallocIfNeeded();
}

sub iosSimulatorApplicationsPath()
{
    return File::Spec->catdir(XcodeSDKPath(), "Applications");
}

sub installedMobileSafariBundle()
{
    return File::Spec->catfile(iosSimulatorApplicationsPath(), "MobileSafari.app");
}

sub mobileSafariBundle()
{
    determineConfigurationProductDir();

    # Use MobileSafari.app in product directory if present.
    if (isAppleMacWebKit() && -d "$configurationProductDir/MobileSafari.app") {
        return "$configurationProductDir/MobileSafari.app";
    }
    return installedMobileSafariBundle();
}

sub plistPathFromBundle($)
{
    my ($appBundle) = @_;
    return "$appBundle/Info.plist" if -f "$appBundle/Info.plist"; # iOS app bundle
    return "$appBundle/Contents/Info.plist" if -f "$appBundle/Contents/Info.plist"; # Mac app bundle
    return "";
}

sub appIdentifierFromBundle($)
{
    my ($appBundle) = @_;
    my $plistPath = File::Spec->rel2abs(plistPathFromBundle($appBundle)); # defaults(1) will complain if the specified path is not absolute.
    chomp(my $bundleIdentifier = `defaults read '$plistPath' CFBundleIdentifier 2> /dev/null`);
    return $bundleIdentifier;
}

sub appDisplayNameFromBundle($)
{
    my ($appBundle) = @_;
    my $plistPath = File::Spec->rel2abs(plistPathFromBundle($appBundle)); # defaults(1) will complain if the specified path is not absolute.
    chomp(my $bundleDisplayName = `defaults read '$plistPath' CFBundleDisplayName 2> /dev/null`);
    return $bundleDisplayName;
}

sub waitUntilIOSSimulatorDeviceIsInState($$)
{
    my ($deviceUDID, $waitUntilState) = @_;
    my $device = iosSimulatorDeviceByUDID($deviceUDID);
    while ($device->{state} ne $waitUntilState) {
        usleep(500 * 1000); # Waiting 500ms between file system polls does not make script run-safari feel sluggish.
        $device = iosSimulatorDeviceByUDID($deviceUDID);
    }
}

sub shutDownIOSSimulatorDevice($)
{
    my ($simulatorDevice) = @_;
    system("xcrun --sdk iphonesimulator simctl shutdown $simulatorDevice->{UDID} > /dev/null 2>&1");
}

sub restartIOSSimulatorDevice($)
{
    my ($simulatorDevice) = @_;
    shutDownIOSSimulatorDevice($simulatorDevice);

    exitStatus(system("xcrun", "--sdk", "iphonesimulator", "simctl", "boot", $simulatorDevice->{UDID})) == 0 or die "Failed to boot simulator device $simulatorDevice->{UDID}";
}

sub relaunchIOSSimulator($)
{
    my ($simulatedDevice) = @_;
    quitIOSSimulator($simulatedDevice->{UDID});

    # FIXME: <rdar://problem/20916140> Switch to using CoreSimulator.framework for launching and quitting iOS Simulator
    my $iosSimulatorBundleID = "com.apple.iphonesimulator";
    system("open", "-b", $iosSimulatorBundleID, "--args", "-CurrentDeviceUDID", $simulatedDevice->{UDID}) == 0 or die "Failed to open $iosSimulatorBundleID: $!";

    waitUntilIOSSimulatorDeviceIsInState($simulatedDevice->{UDID}, SIMULATOR_DEVICE_STATE_BOOTED);
}

sub quitIOSSimulator(;$)
{
    my ($waitForShutdownOfSimulatedDeviceUDID) = @_;
    # FIXME: <rdar://problem/20916140> Switch to using CoreSimulator.framework for launching and quitting iOS Simulator
    exitStatus(system {"osascript"} "osascript", "-e", 'tell application id "com.apple.iphonesimulator" to quit') == 0 or die "Failed to quit iOS Simulator: $!";
    if (!defined($waitForShutdownOfSimulatedDeviceUDID)) {
        return;
    }
    # FIXME: We assume that $waitForShutdownOfSimulatedDeviceUDID was not booted using the simctl command line tool.
    #        Otherwise we will spin indefinitely since quiting the iOS Simulator will not shutdown this device. We
    #        should add a maximum time limit to wait for a device to shutdown and either return an error or die()
    #        on expiration of the time limit.
    waitUntilIOSSimulatorDeviceIsInState($waitForShutdownOfSimulatedDeviceUDID, SIMULATOR_DEVICE_STATE_SHUTDOWN);
}

sub iosSimulatorDeviceByName($)
{
    my ($simulatorName) = @_;
    my $simulatorRuntime = iosSimulatorRuntime();
    my @devices = iOSSimulatorDevices();
    for my $device (@devices) {
        if ($device->{name} eq $simulatorName && $device->{runtime} eq $simulatorRuntime) {
            return $device;
        }
    }
    return undef;
}

sub iosSimulatorDeviceByUDID($)
{
    my ($simulatedDeviceUDID) = @_;
    my $devicePlistPath = File::Spec->catfile(iOSSimulatorDevicesPath(), $simulatedDeviceUDID, "device.plist");
    if (!-f $devicePlistPath) {
        return;
    }
    # FIXME: We should parse the device.plist file ourself and map the dictionary keys in it to known
    #        dictionary keys so as to decouple our representation of the plist from the actual structure
    #        of the plist, which may change.
    eval "require Foundation";
    return Foundation::perlRefFromObjectRef(NSDictionary->dictionaryWithContentsOfFile_($devicePlistPath));
}

sub iosSimulatorRuntime()
{
    my $xcodeSDKVersion = xcodeSDKVersion();
    $xcodeSDKVersion =~ s/\./-/;
    return "com.apple.CoreSimulator.SimRuntime.iOS-$xcodeSDKVersion";
}

sub findOrCreateSimulatorForIOSDevice($)
{
    my ($simulatorNameSuffix) = @_;
    my $simulatorName;
    my $simulatorDeviceType;
    if (architecture() eq "x86_64") {
        $simulatorName = "iPhone 5s " . $simulatorNameSuffix;
        $simulatorDeviceType = "com.apple.CoreSimulator.SimDeviceType.iPhone-5s";
    } else {
        $simulatorName = "iPhone 5 " . $simulatorNameSuffix;
        $simulatorDeviceType = "com.apple.CoreSimulator.SimDeviceType.iPhone-5";
    }
    my $simulatedDevice = iosSimulatorDeviceByName($simulatorName);
    return $simulatedDevice if $simulatedDevice;
    return createiOSSimulatorDevice($simulatorName, $simulatorDeviceType, iosSimulatorRuntime());
}

sub isIOSSimulatorSystemInstalledApp($)
{
    my ($appBundle) = @_;
    my $simulatorApplicationsPath = realpath(iosSimulatorApplicationsPath());
    return substr(realpath($appBundle), 0, length($simulatorApplicationsPath)) eq $simulatorApplicationsPath;
}

sub hasUserInstalledAppInSimulatorDevice($$)
{
    my ($appIdentifier, $simulatedDeviceUDID) = @_;
    my $userInstalledAppPath = File::Spec->catfile($ENV{HOME}, "Library", "Developer", "CoreSimulator", "Devices", $simulatedDeviceUDID, "data", "Containers", "Bundle", "Application");
    if (!-d $userInstalledAppPath) {
        return 0; # No user installed apps.
    }
    local @::userInstalledAppBundles;
    my $wantedFunction = sub {
        my $file = $_;

        # Ignore hidden files and directories.
        if ($file =~ /^\../) {
            $File::Find::prune = 1;
            return;
        }

        return if !-d $file || $file !~ /\.app$/;
        push @::userInstalledAppBundles, $File::Find::name;
        $File::Find::prune = 1; # Do not traverse contents of app bundle.
    };
    find($wantedFunction, $userInstalledAppPath);
    for my $userInstalledAppBundle (@::userInstalledAppBundles) {
        if (appIdentifierFromBundle($userInstalledAppBundle) eq $appIdentifier) {
            return 1; # Has user installed app.
        }
    }
    return 0; # Does not have user installed app.
}

sub isSimulatorDeviceBooted($)
{
    my ($simulatedDeviceUDID) = @_;
    my $device = iosSimulatorDeviceByUDID($simulatedDeviceUDID);
    return $device && $device->{state} eq SIMULATOR_DEVICE_STATE_BOOTED;
}

sub runIOSWebKitAppInSimulator($;$)
{
    my ($appBundle, $simulatorOptions) = @_;
    my $productDir = productDir();
    my $appDisplayName = appDisplayNameFromBundle($appBundle);
    my $appIdentifier = appIdentifierFromBundle($appBundle);
    my $simulatedDevice = findOrCreateSimulatorForIOSDevice(SIMULATOR_DEVICE_SUFFIX_FOR_WEBKIT_DEVELOPMENT);
    my $simulatedDeviceUDID = $simulatedDevice->{UDID};

    my $willUseSystemInstalledApp = isIOSSimulatorSystemInstalledApp($appBundle);
    if ($willUseSystemInstalledApp) {
        if (hasUserInstalledAppInSimulatorDevice($appIdentifier, $simulatedDeviceUDID)) {
            # Restore the system-installed app in the simulator device corresponding to $appBundle as it
            # was previously overwritten with a custom built version of the app.
            # FIXME: Only restore the system-installed version of the app instead of erasing all contents and settings.
            print "Quitting iOS Simulator...\n";
            quitIOSSimulator($simulatedDeviceUDID);
            print "Erasing contents and settings for simulator device \"$simulatedDevice->{name}\".\n";
            exitStatus(system("xcrun", "--sdk", "iphonesimulator", "simctl", "erase", $simulatedDeviceUDID)) == 0 or die;
        }
        # FIXME: We assume that if $simulatedDeviceUDID is not booted then iOS Simulator is not open. However
        #        $simulatedDeviceUDID may have been booted using the simctl command line tool. If $simulatedDeviceUDID
        #        was booted using simctl then we should shutdown the device and launch iOS Simulator to boot it again.
        if (!isSimulatorDeviceBooted($simulatedDeviceUDID)) {
            print "Launching iOS Simulator...\n";
            relaunchIOSSimulator($simulatedDevice);
        }
    } else {
        # FIXME: We should killall(1) any running instances of $appBundle before installing it to ensure
        #        that simctl launch opens the latest installed version of the app. For now we quit and
        #        launch the iOS Simulator again to ensure there are no running instances of $appBundle.
        print "Quitting and launching iOS Simulator...\n";
        relaunchIOSSimulator($simulatedDevice);

        print "Installing $appBundle.\n";
        # Install custom built app, overwriting an app with the same app identifier if one exists.
        exitStatus(system("xcrun", "--sdk", "iphonesimulator", "simctl", "install", $simulatedDeviceUDID, $appBundle)) == 0 or die;

    }

    $simulatorOptions = {} unless $simulatorOptions;

    my %simulatorENV;
    %simulatorENV = %{$simulatorOptions->{applicationEnvironment}} if $simulatorOptions->{applicationEnvironment};
    {
        local %ENV; # Shadow global-scope %ENV so that changes to it will not be seen outside of this scope.
        setupIOSWebKitEnvironment($productDir);
        %simulatorENV = %ENV;
    }
    my $applicationArguments = \@ARGV;
    $applicationArguments = $simulatorOptions->{applicationArguments} if $simulatorOptions && $simulatorOptions->{applicationArguments};

    # Prefix the environment variables with SIMCTL_CHILD_ per `xcrun simctl help launch`.
    foreach my $key (keys %simulatorENV) {
        $ENV{"SIMCTL_CHILD_$key"} = $simulatorENV{$key};
    }

    print "Starting $appDisplayName with DYLD_FRAMEWORK_PATH set to point to built WebKit in $productDir.\n";
    return exitStatus(system("xcrun", "--sdk", "iphonesimulator", "simctl", "launch", $simulatedDeviceUDID, $appIdentifier, @$applicationArguments));
}

sub runIOSWebKitApp($)
{
    my ($appBundle) = @_;
    if (willUseIOSDeviceSDK()) {
        die "Only running Safari in iOS Simulator is supported now.";
    }
    if (willUseIOSSimulatorSDK()) {
        return runIOSWebKitAppInSimulator($appBundle);
    }
    die "Not using an iOS SDK."
}

sub runMacWebKitApp($;$)
{
    my ($appPath, $useOpenCommand) = @_;
    my $productDir = productDir();
    print "Starting @{[basename($appPath)]} with DYLD_FRAMEWORK_PATH set to point to built WebKit in $productDir.\n";

    local %ENV = %ENV;
    setupMacWebKitEnvironment($productDir);

    if (defined($useOpenCommand) && $useOpenCommand == USE_OPEN_COMMAND) {
        return system("open", "-W", "-a", $appPath, "--args", argumentsForRunAndDebugMacWebKitApp());
    }
    if (architecture()) {
        return system "arch", "-" . architecture(), $appPath, argumentsForRunAndDebugMacWebKitApp();
    }
    return system { $appPath } $appPath, argumentsForRunAndDebugMacWebKitApp();
}

sub execMacWebKitAppForDebugging($)
{
    my ($appPath) = @_;
    my $architectureSwitch;
    my $argumentsSeparator;

    if (debugger() eq "lldb") {
        $architectureSwitch = "--arch";
        $argumentsSeparator = "--";
    } elsif (debugger() eq "gdb") {
        $architectureSwitch = "-arch";
        $argumentsSeparator = "--args";
    } else {
        die "Unknown debugger $debugger.\n";
    }

    my $debuggerPath = `xcrun -find $debugger`;
    chomp $debuggerPath;
    die "Can't find the $debugger executable.\n" unless -x $debuggerPath;

    my $productDir = productDir();
    setupMacWebKitEnvironment($productDir);

    my @architectureFlags = ($architectureSwitch, architecture());
    print "Starting @{[basename($appPath)]} under $debugger with DYLD_FRAMEWORK_PATH set to point to built WebKit in $productDir.\n";
    exec { $debuggerPath } $debuggerPath, @architectureFlags, $argumentsSeparator, $appPath, argumentsForRunAndDebugMacWebKitApp() or die;
}

sub debugSafari
{
    if (isAppleMacWebKit()) {
        checkFrameworks();
        execMacWebKitAppForDebugging(safariPath());
    }

    return 1; # Unsupported platform; can't debug Safari on this platform.
}

sub runSafari
{
    if (isIOSWebKit()) {
        return runIOSWebKitApp(mobileSafariBundle());
    }

    if (isAppleMacWebKit()) {
        return runMacWebKitApp(safariPath());
    }

    if (isAppleWinWebKit()) {
        my $result;
        my $webKitLauncherPath = File::Spec->catfile(executableProductDir(), "MiniBrowser.exe");
        return system { $webKitLauncherPath } $webKitLauncherPath, @ARGV;
    }

    return 1; # Unsupported platform; can't run Safari on this platform.
}

sub runMiniBrowser
{
    if (isAppleMacWebKit()) {
        return runMacWebKitApp(File::Spec->catfile(productDir(), "MiniBrowser.app", "Contents", "MacOS", "MiniBrowser"));
    } elsif (isAppleWinWebKit()) {
        my $result;
        my $webKitLauncherPath = File::Spec->catfile(executableProductDir(), "MiniBrowser.exe");
        return system { $webKitLauncherPath } $webKitLauncherPath, @ARGV;
    }

    return 1;
}

sub debugMiniBrowser
{
    if (isAppleMacWebKit()) {
        execMacWebKitAppForDebugging(File::Spec->catfile(productDir(), "MiniBrowser.app", "Contents", "MacOS", "MiniBrowser"));
    }
    
    return 1;
}

sub runWebKitTestRunner
{
    if (isAppleMacWebKit()) {
        return runMacWebKitApp(File::Spec->catfile(productDir(), "WebKitTestRunner"));
    }

    return 1;
}

sub debugWebKitTestRunner
{
    if (isAppleMacWebKit()) {
        execMacWebKitAppForDebugging(File::Spec->catfile(productDir(), "WebKitTestRunner"));
    }

    return 1;
}

sub readRegistryString
{
    my ($valueName) = @_;
    chomp(my $string = `regtool --wow32 get "$valueName"`);
    return $string;
}

sub writeRegistryString
{
    my ($valueName, $string) = @_;

    my $error = system "regtool", "--wow32", "set", "-s", $valueName, $string;

    # On Windows Vista/7 with UAC enabled, regtool will fail to modify the registry, but will still
    # return a successful exit code. So we double-check here that the value we tried to write to the
    # registry was really written.
    return !$error && readRegistryString($valueName) eq $string;
}

sub formatBuildTime($)
{
    my ($buildTime) = @_;

    my $buildHours = int($buildTime / 3600);
    my $buildMins = int(($buildTime - $buildHours * 3600) / 60);
    my $buildSecs = $buildTime - $buildHours * 3600 - $buildMins * 60;

    if ($buildHours) {
        return sprintf("%dh:%02dm:%02ds", $buildHours, $buildMins, $buildSecs);
    }
    return sprintf("%02dm:%02ds", $buildMins, $buildSecs);
}

sub runSvnUpdateAndResolveChangeLogs(@)
{
    my @svnOptions = @_;
    my $openCommand = "svn update " . join(" ", @svnOptions);
    open my $update, "$openCommand |" or die "cannot execute command $openCommand";
    my @conflictedChangeLogs;
    while (my $line = <$update>) {
        print $line;
        $line =~ m/^C\s+(.+?)[\r\n]*$/;
        if ($1) {
          my $filename = normalizePath($1);
          push @conflictedChangeLogs, $filename if basename($filename) eq "ChangeLog";
        }
    }
    close $update or die;

    if (@conflictedChangeLogs) {
        print "Attempting to merge conflicted ChangeLogs.\n";
        my $resolveChangeLogsPath = File::Spec->catfile(sourceDir(), "Tools", "Scripts", "resolve-ChangeLogs");
        (system($resolveChangeLogsPath, "--no-warnings", @conflictedChangeLogs) == 0)
            or die "Could not open resolve-ChangeLogs script: $!.\n";
    }
}

sub runGitUpdate()
{
    # Doing a git fetch first allows setups with svn-remote.svn.fetch = trunk:refs/remotes/origin/master
    # to perform the rebase much much faster.
    system("git", "fetch");
    if (isGitSVNDirectory(".")) {
        system("git", "svn", "rebase") == 0 or die;
    } else {
        # This will die if branch.$BRANCHNAME.merge isn't set, which is
        # almost certainly what we want.
        system("git", "pull") == 0 or die;
    }
}

1;
