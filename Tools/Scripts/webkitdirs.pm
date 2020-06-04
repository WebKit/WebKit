# Copyright (C) 2005-2019 Apple Inc. All rights reserved.
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
use File::Temp qw(tempdir);
use File::stat;
use JSON::PP;
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
       &archCommandLineArgumentsForRestrictedEnvironmentVariables
       &availableXcodeSDKs
       &baseProductDir
       &chdirWebKit
       &checkFrameworks
       &cmakeArgsFromFeatures
       &currentSVNRevision
       &debugSafari
       &executableProductDir
       &extractNonHostConfiguration
       &iosVersion
       &nmPath
       &passedConfiguration
       &prependToEnvironmentVariableList
       &printHelpAndExitForRunAndDebugWebKitAppIfNeeded
       &productDir
       &runIOSWebKitApp
       &runMacWebKitApp
       &safariPath
       &sdkDirectory
       &sdkPlatformDirectory
       &setConfiguration
       &setupMacWebKitEnvironment
       &setupUnixWebKitEnvironment
       &sharedCommandLineOptions
       &sharedCommandLineOptionsUsage
       &shouldUseFlatpak
       &runInFlatpak
       &sourceDir
       &willUseIOSDeviceSDK
       &willUseIOSSimulatorSDK
       DO_NOT_USE_OPEN_COMMAND
       USE_OPEN_COMMAND
   );
   %EXPORT_TAGS = ( );
   @EXPORT_OK   = ();
}

# Ports
use constant {
    AppleWin    => "AppleWin",
    FTW         => "FTW",
    GTK         => "GTK",
    iOS         => "iOS",
    tvOS        => "tvOS",
    watchOS     => "watchOS",
    Mac         => "Mac",
    MacCatalyst => "MacCatalyst",
    JSCOnly     => "JSCOnly",
    PlayStation => "PlayStation",
    WinCairo    => "WinCairo",
    WPE         => "WPE",
    Unknown     => "Unknown"
};

use constant USE_OPEN_COMMAND => 1; # Used in runMacWebKitApp().
use constant DO_NOT_USE_OPEN_COMMAND => 2;
use constant SIMULATOR_DEVICE_STATE_SHUTDOWN => "Shutdown";
use constant SIMULATOR_DEVICE_STATE_BOOTED => "Booted";
use constant SIMULATOR_DEVICE_SUFFIX_FOR_WEBKIT_DEVELOPMENT  => "For WebKit Development";

# See table "Certificate types and names" on <https://developer.apple.com/library/ios/documentation/IDEs/Conceptual/AppDistributionGuide/MaintainingCertificates/MaintainingCertificates.html#//apple_ref/doc/uid/TP40012582-CH31-SW41>.
use constant IOS_DEVELOPMENT_CERTIFICATE_NAME_PREFIX => "iPhone Developer: ";

our @EXPORT_OK;

my $architecture;
my $asanIsEnabled;
my $forceOptimizationLevel;
my $coverageIsEnabled;
my $ltoMode;
my $numberOfCPUs;
my $maxCPULoad;
my $baseProductDir;
my @baseProductDirOption;
my $configuration;
my $xcodeSDK;
my $simulatorIdiom;
my $configurationForVisualStudio;
my $configurationProductDir;
my $sourceDir;
my $currentSVNRevision;
my $didLoadIPhoneSimulatorNotification;
my $nmPath;
my $osXVersion;
my $iosVersion;
my $generateDsym;
my $isCMakeBuild;
my $isGenerateProjectOnly;
my $shouldBuild32Bit;
my $isWin64;
my $isInspectorFrontend;
my $portName;
my $shouldUseGuardMalloc;
my $shouldNotUseNinja;
my $xcodeVersion;

my $unknownPortProhibited = 0;

# Variables for Win32 support
my $programFilesPath;
my $msBuildPath;
my $vsInstallDir;
my $vsVersion;
my $windowsSourceDir;
my $winVersion;

# Defined in VCSUtils.
sub exitStatus($);

sub findMatchingArguments($$);
sub hasArgument($$);

sub sdkDirectory($)
{
    my ($sdkName) = @_;
    chomp(my $sdkDirectory = `xcrun --sdk '$sdkName' --show-sdk-path`);
    die "Failed to get SDK path from xcrun: $!" if exitStatus($?);
    return $sdkDirectory;
}

sub sdkPlatformDirectory($)
{
    my ($sdkName) = @_;
    chomp(my $sdkPlatformDirectory = `xcrun --sdk '$sdkName' --show-sdk-platform-path`);
    die "Failed to get SDK platform path from xcrun: $!" if exitStatus($?);
    return $sdkPlatformDirectory;
}

sub determineSourceDir
{
    return if $sourceDir;
    $sourceDir = $FindBin::Bin;
    $sourceDir =~ s|/+$||; # Remove trailing '/' as we would die later

    # walks up path checking each directory to see if it is the main WebKit project dir, 
    # defined by containing Sources, WebCore, and JavaScriptCore.
    until ((-d File::Spec->catdir($sourceDir, "Source") && -d File::Spec->catdir($sourceDir, "Source", "WebCore") && -d File::Spec->catdir($sourceDir, "Source", "JavaScriptCore")) || (-d File::Spec->catdir($sourceDir, "Internal") && -d File::Spec->catdir($sourceDir, "OpenSource")))
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
    $xcodeVersion = ($xcodebuildVersionOutput =~ /Xcode ([0-9]+(\.[0-9]+)*)/) ? $1 : "3.0";
}

sub readXcodeUserDefault($)
{
    my ($key) = @_;

    my $devnull = File::Spec->devnull();

    my $value = `defaults read com.apple.dt.Xcode ${key} 2> ${devnull}`;
    return if $?;

    chomp $value;
    return $value;
}

sub determineBaseProductDir
{
    return if defined $baseProductDir;
    determineSourceDir();

    my $setSharedPrecompsDir;
    my $indexDataStoreDir;
    $baseProductDir = $ENV{"WEBKIT_OUTPUTDIR"};

    if (!defined($baseProductDir) and isAppleCocoaWebKit()) {
        # Silently remove ~/Library/Preferences/xcodebuild.plist which can
        # cause build failure. The presence of
        # ~/Library/Preferences/xcodebuild.plist can prevent xcodebuild from
        # respecting global settings such as a custom build products directory
        # (<rdar://problem/5585899>).
        my $personalPlistFile = $ENV{HOME} . "/Library/Preferences/xcodebuild.plist";
        if (-e $personalPlistFile) {
            unlink($personalPlistFile) || die "Could not delete $personalPlistFile: $!";
        }

        my $buildLocationStyle = join '', readXcodeUserDefault("IDEBuildLocationStyle");
        if ($buildLocationStyle eq "Custom") {
            my $buildLocationType = join '', readXcodeUserDefault("IDECustomBuildLocationType");
            # FIXME: Read CustomBuildIntermediatesPath and set OBJROOT accordingly.
            if ($buildLocationType eq "Absolute") {
                $baseProductDir = readXcodeUserDefault("IDECustomBuildProductsPath");
                $indexDataStoreDir = readXcodeUserDefault("IDECustomIndexStorePath");
            }
        }

        # DeterminedByTargets corresponds to a setting of "Legacy" in Xcode.
        # It is the only build location style for which SHARED_PRECOMPS_DIR is not
        # overridden when building from within Xcode.
        $setSharedPrecompsDir = 1 if $buildLocationStyle ne "DeterminedByTargets";

        if (!defined($baseProductDir)) {
            $baseProductDir = join '', readXcodeUserDefault("IDEApplicationwideBuildSettings");
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

    if (isAppleCocoaWebKit()) {
        $baseProductDir =~ s|^\Q$(SRCROOT)/..\E$|$sourceDir|;
        $baseProductDir =~ s|^\Q$(SRCROOT)/../|$sourceDir/|;
        $baseProductDir =~ s|^~/|$ENV{HOME}/|;
        die "Can't handle Xcode product directory with a ~ in it.\n" if $baseProductDir =~ /~/;
        die "Can't handle Xcode product directory with a variable in it.\n" if $baseProductDir =~ /\$/;
        @baseProductDirOption = ("SYMROOT=$baseProductDir", "OBJROOT=$baseProductDir");
        push(@baseProductDirOption, "SHARED_PRECOMPS_DIR=${baseProductDir}/PrecompiledHeaders") if $setSharedPrecompsDir;
        push(@baseProductDirOption, "INDEX_ENABLE_DATA_STORE=YES", "INDEX_DATA_STORE_DIR=${indexDataStoreDir}") if $indexDataStoreDir;
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

sub systemVerbose {
    print "+ @_\n";
    return system(@_);
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

    if (isAppleCocoaWebKit()) {
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
                $architecture = 'arm64';
            } elsif ($xcodeSDK =~ /^watchsimulator/) {
                $architecture = 'i386';
            } elsif ($xcodeSDK =~ /^watchos/) {
                $architecture = 'arm64_32 arm64e armv7k';
            } elsif ($xcodeSDK =~ /^appletvsimulator/) {
                $architecture = 'x86_64';
            } elsif ($xcodeSDK =~ /^appletvos/) {
                $architecture = 'arm64';
            }
        }
    } elsif (isCMakeBuild()) {
        if (isCrossCompilation()) {
            my $compiler = "gcc";
            $compiler = $ENV{'CC'} if (defined($ENV{'CC'}));
            my @compiler_machine = split('-', `$compiler -dumpmachine`);
            $architecture = $compiler_machine[0];
        } elsif (open my $cmake_sysinfo, "cmake --system-information |") {
            while (<$cmake_sysinfo>) {
                next unless index($_, 'CMAKE_SYSTEM_PROCESSOR') == 0;
                if (/^CMAKE_SYSTEM_PROCESSOR \"([^"]+)\"/) {
                    $architecture = $1;
                    last;
                }
            }
            close $cmake_sysinfo;
        }
    }

    if (!isAnyWindows()) {
        if (!$architecture) {
            # Fall back to output of `uname -m', if it is present.
            $architecture = `uname -m`;
            chomp $architecture;
        }
    }

    $architecture = 'x86_64' if $architecture =~ /amd64/i;
    $architecture = 'arm64' if $architecture =~ /aarch64/i;
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

sub determineForceOptimizationLevel
{
    return if defined $forceOptimizationLevel;
    determineBaseProductDir();

    if (open ForceOptimizationLevel, "$baseProductDir/ForceOptimizationLevel") {
        $forceOptimizationLevel = <ForceOptimizationLevel>;
        close ForceOptimizationLevel;
        chomp $forceOptimizationLevel;
    }
}

sub determineCoverageIsEnabled
{
    return if defined $coverageIsEnabled;
    determineBaseProductDir();

    if (open Coverage, "$baseProductDir/Coverage") {
        $coverageIsEnabled = <Coverage>;
        close Coverage;
        chomp $coverageIsEnabled;
    }
}

sub determineLTOMode
{
    return if defined $ltoMode;
    determineBaseProductDir();

    if (open LTO, "$baseProductDir/LTO") {
        $ltoMode = <LTO>;
        close LTO;
        chomp $ltoMode;
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
    } else {
        $numberOfCPUs = 1;
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
    if (isPlayStation()) {
        $jscName .= ".elf";
    } elsif (isAnyWindows()) {
        $jscName .= ".exe";
    }
    return "$productDir/$jscName" if -e "$productDir/$jscName";
    return "$productDir/JavaScriptCore.framework/Helpers/$jscName";
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
    push(@args, '--ios-device') if (defined $xcodeSDK && $xcodeSDK =~ /^iphoneos/);
    push(@args, '--ios-simulator') if (defined $xcodeSDK && $xcodeSDK =~ /^iphonesimulator/ && $simulatorIdiom eq "iPhone");
    push(@args, '--ipad-simulator') if (defined $xcodeSDK && $xcodeSDK =~ /^iphonesimulator/ && $simulatorIdiom eq "iPad");
    push(@args, '--tvos-device') if (defined $xcodeSDK && $xcodeSDK =~ /^appletvos/);
    push(@args, '--tvos-simulator') if (defined $xcodeSDK && $xcodeSDK =~ /^appletvsimulator/);
    push(@args, '--watchos-device') if (defined $xcodeSDK && $xcodeSDK =~ /^watchos/);
    push(@args, '--watchos-simulator') if (defined $xcodeSDK && $xcodeSDK =~ /^watchsimulator/);
    push(@args, '--maccatalyst') if (defined $xcodeSDK && $xcodeSDK =~ /^maccatalyst/);
    push(@args, '--32-bit') if ($architecture eq "x86" and !isWin64());
    push(@args, '--64-bit') if (isWin64());
    push(@args, '--ftw') if isFTW();
    push(@args, '--gtk') if isGtk();
    push(@args, '--wpe') if isWPE();
    push(@args, '--jsc-only') if isJSCOnly();
    push(@args, '--wincairo') if isWinCairo();
    push(@args, '--playstation') if isPlayStation();
    return @args;
}

sub extractNonMacOSHostConfiguration
{
    my @args = ();
    my @extract = ('--device', '--gtk', '--ios', '--platform', '--sdk', '--simulator', '--wincairo', '--ftw', '--tvos', '--watchos', 'SDKROOT', 'ARCHS');
    foreach (@{$_[0]}) {
        my $line = $_;
        my $flag = 0;
        foreach (@extract) {
            if (length($line) >= length($_) && substr($line, 0, length($_)) eq $_
                && index($line, 'i386') == -1 && index($line, 'x86_64') == -1) {
                $flag = 1;
            }
        }
        if (!$flag) {
            push @args, $_;
        }
    }
    return @args;
}

# FIXME: Convert to json <rdar://problem/21594308>
sub parseAvailableXcodeSDKs($)
{
    my @outputToParse = @{$_[0]};
    my @result = ();
    foreach my $line (@outputToParse) {
        # Examples:
        #    iOS 12.0 -sdk iphoneos12.0
        #    Simulator - iOS 12.0 -sdk iphonesimulator12.0
        #    macOS 10.14 -sdk macosx10.14
        if ($line =~ /-sdk (\D+)([\d\.]+)(\D*)\n/) {
            if ($3) {
                push @result, "$1.$3";
            } else {
                push @result, "$1";
            }
        }
    }
    return @result;
}

sub availableXcodeSDKs
{
    my @output = `xcodebuild -showsdks`;
    return parseAvailableXcodeSDKs(\@output);
}

sub determineXcodeSDK
{
    return if defined $xcodeSDK;
    my $sdk;
    
    # The user explicitly specified the sdk, don't assume anything
    if (checkForArgumentAndRemoveFromARGVGettingValue("--sdk", \$sdk)) {
        $xcodeSDK = $sdk;
        return;
    }
    if (checkForArgumentAndRemoveFromARGV("--device") || checkForArgumentAndRemoveFromARGV("--ios-device")) {
        $xcodeSDK ||= "iphoneos";
    }
    if (checkForArgumentAndRemoveFromARGV("--simulator") || checkForArgumentAndRemoveFromARGV("--ios-simulator")) {
        $xcodeSDK ||= 'iphonesimulator';
        $simulatorIdiom = 'iPhone';
    }
    if (checkForArgumentAndRemoveFromARGV("--ipad-simulator")) {
        $xcodeSDK ||= 'iphonesimulator';
        $simulatorIdiom = 'iPad';
    }
    if (checkForArgumentAndRemoveFromARGV("--tvos-device")) {
        $xcodeSDK ||=  "appletvos";
    }
    if (checkForArgumentAndRemoveFromARGV("--tvos-simulator")) {
        $xcodeSDK ||= "appletvsimulator";
    }
    if (checkForArgumentAndRemoveFromARGV("--watchos-device")) {
        $xcodeSDK ||=  "watchos";
    }
    if (checkForArgumentAndRemoveFromARGV("--watchos-simulator")) {
        $xcodeSDK ||= "watchsimulator";
    }
    if (checkForArgumentAndRemoveFromARGV("--maccatalyst")) {
        $xcodeSDK ||= "maccatalyst";
    }
    return if !defined $xcodeSDK;
    
    # Prefer the internal version of an sdk, if it exists.
    my @availableSDKs = availableXcodeSDKs();

    foreach my $sdk (@availableSDKs) {
        next if $sdk ne "$xcodeSDK.internal";
        $xcodeSDK = $sdk;
        last;
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
    return "appletvos" if $xcodeSDK =~ /appletvos/i;
    return "appletvsimulator" if $xcodeSDK =~ /appletvsimulator/i;
    return "iphoneos" if $xcodeSDK =~ /iphoneos/i;
    return "iphonesimulator" if $xcodeSDK =~ /iphonesimulator/i;
    return "macosx" if $xcodeSDK =~ /macosx/i;
    return "watchos" if $xcodeSDK =~ /watchos/i;
    return "watchsimulator" if $xcodeSDK =~ /watchsimulator/i;
    return "maccatalyst" if $xcodeSDK =~ /maccatalyst/i;
    die "Couldn't determine platform name from Xcode SDK";
}

sub XcodeSDKPath
{
    determineXcodeSDK();

    die "Can't find the SDK path because no Xcode SDK was specified" if not $xcodeSDK;
    return sdkDirectory($xcodeSDK);
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

sub programFilesPathX86
{
    my $programFilesPathX86 = $ENV{'PROGRAMFILES(X86)'} || "C:\\Program Files (x86)";

    return $programFilesPathX86;
}

sub visualStudioInstallDirVSWhere
{
    my $vswhere = File::Spec->catdir(programFilesPathX86(), "Microsoft Visual Studio", "Installer", "vswhere.exe");
    return unless -e $vswhere;
    open(my $handle, "-|", $vswhere, qw(-nologo -latest -requires Microsoft.Component.MSBuild -property installationPath)) || return;
    my $vsWhereOut = <$handle>;
    $vsWhereOut =~ s/\r?\n//;
    return $vsWhereOut;
}

sub visualStudioInstallDir
{
    return $vsInstallDir if defined $vsInstallDir;

    if ($ENV{'VSINSTALLDIR'}) {
        $vsInstallDir = $ENV{'VSINSTALLDIR'};
        $vsInstallDir =~ s|[\\/]$||;
    } else {
        $vsInstallDir = visualStudioInstallDirVSWhere();
        return unless defined $vsInstallDir;
    }
    chomp($vsInstallDir = `cygpath "$vsInstallDir"`) if isCygwin();

    print "Using Visual Studio: $vsInstallDir\n";
    return $vsInstallDir;
}

sub msBuildPath
{
    my $installDir = visualStudioInstallDir();

    # FIXME: vswhere.exe should be used to find msbuild.exe after AppleWin will get vswhere with -find switch.
    # <https://github.com/Microsoft/vswhere/wiki/Find-MSBuild>
    # <https://github.com/Microsoft/vswhere/releases/tag/2.6.6%2Bd9dbe79db3>
    my $path = File::Spec->catdir($installDir, "MSBuild", "Current", "bin", "MSBuild.exe");
    $path = File::Spec->catdir($installDir, "MSBuild", "15.0", "bin", "MSBuild.exe") unless -e $path;

    chomp($path = `cygpath "$path"`) if isCygwin();

    print "Using MSBuild: $path\n";
    return $path;
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
    return ($ENV{"WEBKIT_OUTPUTDIR"} && isGtk()) || isAppleWinWebKit() || isFTW();
}

sub determineConfigurationProductDir
{
    return if defined $configurationProductDir;
    determineBaseProductDir();
    determineConfiguration();
    if (isAppleWinWebKit() || isWinCairo() || isPlayStation() || isFTW()) {
        $configurationProductDir = File::Spec->catdir($baseProductDir, $configuration);
    } else {
        if (usesPerConfigurationBuildDirectory()) {
            $configurationProductDir = "$baseProductDir";
        } else {
            if (shouldUseFlatpak()) {
                $configurationProductDir = "$baseProductDir/$portName/$configuration";
            } else {
                $configurationProductDir = "$baseProductDir/$configuration";
            }
            $configurationProductDir .= "-" . xcodeSDKPlatformName() if isEmbeddedWebKit() || isMacCatalystWebKit();
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
    if (isAnyWindows() && !isPlayStation()) {
        $binaryDirectory = isWin64() ? "bin64" : "bin32";
    } elsif (isGtk() || isJSCOnly() || isWPE() || isPlayStation()) {
        $binaryDirectory = "bin";
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

sub forceOptimizationLevel()
{
    determineForceOptimizationLevel();
    return $forceOptimizationLevel;
}

sub ltoMode()
{
    determineLTOMode();
    return $ltoMode;
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
    determineForceOptimizationLevel();
    determineCoverageIsEnabled();
    determineLTOMode();
    determineXcodeSDK();

    my @options;
    push @options, "-UseSanitizedBuildSystemEnvironment=YES";
    push @options, "-ShowBuildOperationDuration=YES";
    push @options, ("-configuration", $configuration);
    push @options, ("-xcconfig", sourceDir() . "/Tools/asan/asan.xcconfig", "ASAN_IGNORE=" . sourceDir() . "/Tools/asan/webkit-asan-ignore.txt") if $asanIsEnabled;
    push @options, ("-xcconfig", sourceDir() . "/Tools/coverage/coverage.xcconfig") if $coverageIsEnabled;
    push @options, ("GCC_OPTIMIZATION_LEVEL=$forceOptimizationLevel") if $forceOptimizationLevel;
    push @options, "WK_LTO_MODE=$ltoMode" if $ltoMode;
    push @options, @baseProductDirOption;
    push @options, "ARCHS=$architecture" if $architecture;
    push @options, "SDKROOT=$xcodeSDK" if $xcodeSDK;

    # When this environment variable is set Tools/Scripts/check-for-weak-vtables-and-externals
    # treats errors as non-fatal when it encounters missing symbols related to coverage.
    appendToEnvironmentVariableList("WEBKIT_COVERAGE_BUILD", "1") if $coverageIsEnabled;

    die "cannot enable both ASAN and Coverage at this time\n" if $coverageIsEnabled && $asanIsEnabled;

    if (willUseIOSDeviceSDK() || willUseWatchDeviceSDK() || willUseAppleTVDeviceSDK()) {
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

sub canUseXCBuild()
{
    determineXcodeVersion();
    return (eval "v$xcodeVersion" ge v11.4)
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
    } elsif(checkForArgumentAndRemoveFromARGV("--testing")) {
        $passedConfiguration = "Testing";
    } elsif(checkForArgumentAndRemoveFromARGV("--release-and-assert") || checkForArgumentAndRemoveFromARGV("--ra")) {
        $passedConfiguration = "Release+Assert";
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
    if (shouldBuild32Bit()) {
        if (isAppleCocoaWebKit()) {
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

# Locate Safari.
sub safariPath
{
    die "Safari path is only relevant on Apple Mac platform\n" unless isAppleMacWebKit();

    my $safariPath;

    # Use WEBKIT_SAFARI environment variable if present.
    my $safariBundle = $ENV{WEBKIT_SAFARI};
    if (!$safariBundle) {
        determineConfigurationProductDir();
        # Use Safari.app in product directory if present (good for Safari development team).
        if (-d "$configurationProductDir/Safari.app") {
            $safariBundle = "$configurationProductDir/Safari.app";
        }
    }

    if ($safariBundle) {
        $safariPath = "$safariBundle/Contents/MacOS/Safari";
    } else {
        $safariPath = "/Applications/Safari.app/Contents/MacOS/SafariForWebKitDevelopment";
    }

    die "Can't find executable at $safariPath.\n" if !-x $safariPath;
    return $safariPath;
}

sub builtDylibPathForName
{
    my $libraryName = shift;
    determineConfigurationProductDir();

    if (isGtk()) {
        my $extension = isDarwin() ? ".dylib" : ".so";
        my @apiVersions = ("4.0", "5.0");
        for my $apiVersion (@apiVersions) {
            my $libraryPath;
            if ($libraryName eq "JavaScriptCore") {
                $libraryPath = "$configurationProductDir/lib/libjavascriptcoregtk-$apiVersion$extension";
            } else {
                $libraryPath = "$configurationProductDir/lib/libwebkit2gtk-$apiVersion$extension";
            }
            if (-e $libraryPath) {
                return $libraryPath;
            }
        }

        return "";
    }
    if (isIOSWebKit()) {
        return "$configurationProductDir/$libraryName.framework/$libraryName";
    }
    if (isAppleCocoaWebKit()) {
        return "$configurationProductDir/$libraryName.framework/Versions/A/$libraryName";
    }
    if (isAppleWinWebKit() || isFTW()) {
        if ($libraryName eq "JavaScriptCore") {
            return "$baseProductDir/lib/$libraryName.lib";
        } else {
            return "$baseProductDir/$libraryName.intermediate/$configuration/$libraryName.intermediate/$libraryName.lib";
        }
    }
    if (isWPE()) {
        return "$configurationProductDir/lib/libWPEWebKit-1.0.so";
    }

    die "Unsupported platform, can't determine built library locations.\nTry `build-webkit --help` for more information.\n";
}

# Check to see that all the frameworks are built.
sub checkFrameworks # FIXME: This is a poor name since only the Mac calls built WebCore a Framework.
{
    return if isAnyWindows();
    my @frameworks = ("JavaScriptCore", "WebCore");
    push(@frameworks, "WebKit") if isAppleCocoaWebKit(); # FIXME: This seems wrong, all ports should have a WebKit these days.
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
        ftw => FTW,
        gtk => GTK,
        'jsc-only' => JSCOnly,
        playstation => PlayStation,
        wincairo => WinCairo,
        wpe => WPE
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
        } elsif (willUseAppleTVDeviceSDK() || willUseAppleTVSimulatorSDK()) {
            $portName = tvOS;
        } elsif (willUseWatchDeviceSDK() || willUseWatchSimulatorSDK()) {
            $portName = watchOS;
        } elsif (willUseMacCatalystSDK()) {
            $portName = MacCatalyst;
        } else {
            $portName = Mac;
        }
    } else {
        if ($unknownPortProhibited) {
            my $portsChoice = join "\n\t", qw(
                --gtk
                --jsc-only
                --wpe
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

sub isGtk()
{
    return portName() eq GTK;
}

sub isJSCOnly()
{
    return portName() eq JSCOnly;
}

sub isWPE()
{
    return portName() eq WPE;
}

sub isPlayStation()
{
    return portName() eq PlayStation;
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

sub isFTW()
{
    return portName() eq FTW;
}

sub isWinCairo()
{
    return portName() eq WinCairo;
}

sub shouldBuild32Bit()
{
    determineShouldBuild32Bit();
    return $shouldBuild32Bit;
}

sub determineShouldBuild32Bit()
{
    return if defined($shouldBuild32Bit);
    $shouldBuild32Bit = checkForArgumentAndRemoveFromARGV("--32-bit");
}

sub isWin64()
{
    determineIsWin64();
    return $isWin64;
}

sub determineIsWin64()
{
    return if defined($isWin64);
    $isWin64 = checkForArgumentAndRemoveFromARGV("--64-bit") || ((isAnyWindows() || isJSCOnly()) && !shouldBuild32Bit());
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
    $versionString =~ /(\d+)\.(\d+)\.(\d+)/;

    $winVersion = {
        major => $1,
        minor => $2,
        subminor => $3,
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

sub isX86_64()
{
    return (architecture() eq "x86_64") || 0;
}

sub isARM64()
{
    return (architecture() eq "arm64") || 0;
}

sub isCrossCompilation()
{
    my $compiler = "";
    $compiler = $ENV{'CC'} if (defined($ENV{'CC'}));
    if ($compiler =~ /gcc/) {
        my $compilerOptions = `$compiler -v 2>&1`;
        my @host = $compilerOptions =~ m/--host=(.*?)\s/;
        my @target = $compilerOptions =~ m/--target=(.*?)\s/;
        if (!@target || !@host) {
            # Sometimes a compiler does not report the host of target it was compiled for,
            # in which case, lacking better information we assume we are not cross-compiling.
            return 0;
        }
        elsif ($target[0] ne "" && $host[0] ne "") {
                return ($host[0] ne $target[0]);
        } else {
                # $tempDir gets automatically deleted when goes out of scope
                my $tempDir = File::Temp->newdir();
                my $testProgramSourcePath = File::Spec->catfile($tempDir, "testcross.c");
                my $testProgramBinaryPath = File::Spec->catfile($tempDir, "testcross");
                open(my $testProgramSourceHandler, ">", $testProgramSourcePath);
                print $testProgramSourceHandler "int main() { return 0; }\n";
                system("$compiler $testProgramSourcePath -o $testProgramBinaryPath > /dev/null 2>&1") == 0 or return 0;
                # Crosscompiling if the program fails to run (because it was built for other arch)
                system("$testProgramBinaryPath > /dev/null 2>&1") == 0 or return 1;
                return 0;
        }
    }
    return 0;
}

sub isIOSWebKit()
{
    return portName() eq iOS;
}

sub isTVOSWebKit()
{
    return portName() eq tvOS;
}

sub isWatchOSWebKit()
{
    return portName() eq watchOS;
}

sub isEmbeddedWebKit()
{
    return isIOSWebKit() || isTVOSWebKit() || isWatchOSWebKit();
}

sub isAppleWebKit()
{
    return isAppleCocoaWebKit() || isAppleWinWebKit();
}

sub isAppleMacWebKit()
{
    return portName() eq Mac;
}

sub isMacCatalystWebKit()
{
    return portName() eq MacCatalyst;
}

sub isAppleCocoaWebKit()
{
    return isAppleMacWebKit() || isEmbeddedWebKit() || isMacCatalystWebKit();
}

sub isAppleWinWebKit()
{
    return portName() eq AppleWin;
}

sub simulatorDeviceFromJSON
{
    my $runtime = shift;
    my $jsonDevice = shift;

    return {
        "UDID" => $jsonDevice->{udid},
        "name" => $jsonDevice->{name},
        "runtime" => $runtime,
        "state" => $jsonDevice->{state},
        "deviceType" => $jsonDevice->{deviceTypeIdentifier}
    };
}

sub iOSSimulatorDevices
{
    my $output = `xcrun simctl list devices --json`;
    my $runtimes = decode_json($output)->{devices};
    if (!$runtimes) {
        die "No simulator devices found";
    }

    my @devices = ();
    while ((my $runtime, my $devicesForRuntime) = each %$runtimes) {
        foreach my $jsonDevice (@$devicesForRuntime) {
            next if $jsonDevice->{availabilityError};
            push @devices, simulatorDeviceFromJSON($runtime, $jsonDevice);
        }
    }

    return @devices;
}

sub createiOSSimulatorDevice
{
    my $name = shift;
    my $deviceTypeId = shift;
    my $runtimeId = shift;

    my $created = system("xcrun", "--sdk", "iphonesimulator", "simctl", "create", $name, $deviceTypeId, $runtimeId) == 0;
    die "Couldn't create simulator device: $name $deviceTypeId $runtimeId" if not $created;

    my @devices = iOSSimulatorDevices();
    foreach my $device (@devices) {
        return $device if $device->{name} eq $name and $device->{deviceType} eq $deviceTypeId and $device->{runtime} eq $runtimeId;
    }

    die "Device $name $deviceTypeId $runtimeId wasn't found";
}

sub willUseIOSDeviceSDK()
{
    return xcodeSDKPlatformName() eq "iphoneos";
}

sub willUseIOSSimulatorSDK()
{
    return xcodeSDKPlatformName() eq "iphonesimulator";
}

sub willUseAppleTVDeviceSDK()
{
    return xcodeSDKPlatformName() eq "appletvos";
}

sub willUseAppleTVSimulatorSDK()
{
    return xcodeSDKPlatformName() eq "appletvsimulator";
}

sub willUseWatchDeviceSDK()
{
    return xcodeSDKPlatformName() eq "watchos";
}

sub willUseWatchSimulatorSDK()
{
    return xcodeSDKPlatformName() eq "watchsimulator";
}

sub willUseMacCatalystSDK()
{
    return xcodeSDKPlatformName() eq "maccatalyst";
}

sub determineNmPath()
{
    return if $nmPath;

    if (isAppleCocoaWebKit()) {
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
    my @subMinorVersion = defined($splitVersion[2]) ? split(/-/, $splitVersion[2]) : (0);
    return {
        "major" => $splitVersion[0],
        "minor" => $splitVersion[1],
        "subminor" => $subMinorVersion[0],
    };
}

sub determineOSXVersion()
{
    return if $osXVersion;

    if (!isDarwin()) {
        $osXVersion = -1;
        return;
    }

    chomp(my $versionString = `sw_vers -productVersion`);
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
    if (isGtk() || isWPE()) {
        if (inFlatpakSandbox()) {
            return "Tools/Scripts/run-minibrowser";
        }
        return "$relativeScriptsPath/run-minibrowser";
    } elsif (isAppleWebKit()) {
        return "$relativeScriptsPath/run-safari";
    }
}

sub launcherName()
{
    if (isGtk() || isWPE()) {
        return "MiniBrowser";
    } elsif (isAppleMacWebKit()) {
        return "Safari";
    } elsif (isAppleWinWebKit() || isFTW()) {
        return "MiniBrowser";
    }
}

sub checkRequiredSystemConfig
{
    if (isDarwin()) {
        chomp(my $productVersion = `sw_vers -productVersion`);
        if (eval "v$productVersion" lt v10.10.5) {
            print "*************************************************************\n";
            print "OS X Yosemite v10.10.5 or later is required to build WebKit.\n";
            print "You have " . $productVersion . ", thus the build will most likely fail.\n";
            print "*************************************************************\n";
        }
        determineXcodeVersion();
        if (eval "v$xcodeVersion" lt v7.0) {
            print "*************************************************************\n";
            print "Xcode 7.0 or later is required to build WebKit.\n";
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
    my $cmd = "reg query \"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts\" /v \"$font\" 2>&1";
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

    # MathML requires fonts that may not ship with Windows.
    # Warn the user if they are missing.
    my @fonts = ('Cambria & Cambria Math (TrueType)', 'LatinModernMath-Regular (TrueType)', 'STIXMath-Regular (TrueType)');
    my @missing = ();
    foreach my $font (@fonts) {
        push @missing, $font if not fontExists($font);
    }

    if (scalar @missing > 0) {
        print "*************************************************************\n";
        print "Mathematical fonts, such as Latin Modern Math are needed to\n";
        print "use the MathML feature.  You do not appear to have these fonts\n";
        print "on your system.\n\n";
        print "You can download a suitable set of fonts from the following URL:\n";
        print "https://trac.webkit.org/wiki/MathML/Fonts\n";
        print "*************************************************************\n";
    }

    print "Installed tools are correct for the WebKit build.\n";
}

sub setupAppleWinEnv()
{
    return unless isAppleWinWebKit() || isFTW();

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
            print "         to be able build WebKit from within Visual Studio 2017 and newer.\n";
            print "         Make sure that 'WebKit_Libraries' points to the\n";
            print "         'WebKitLibraries/win' directory, not the 'WebKitLibraries/' directory.\n\n";
        }
        if (!defined $ENV{'WEBKIT_OUTPUTDIR'} || !$ENV{'WEBKIT_OUTPUTDIR'}) {
            print "Warning: You must set the 'WebKit_OutputDir' environment variable\n";
            print "         to be able build WebKit from within Visual Studio 2017 and newer.\n\n";
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
    return if $msBuildPath;

    my $programFilesPath = programFilesPath();

    print "Building results into: ", baseProductDir(), "\n";
    print "WEBKIT_OUTPUTDIR is set to: ", $ENV{"WEBKIT_OUTPUTDIR"}, "\n";
    print "WEBKIT_LIBRARIES is set to: ", $ENV{"WEBKIT_LIBRARIES"}, "\n";
    # FIXME (125180): Remove the following temporary 64-bit support once official support is available.
    print "WEBKIT_64_SUPPORT is set to: ", $ENV{"WEBKIT_64_SUPPORT"}, "\n" if isWin64();

    # We will actually use MSBuild to build WebKit, but we need to find the Visual Studio install (above) to make
    # sure we use the right options.
    $msBuildPath = msBuildPath();
    if (! -e $msBuildPath) {
        print "*************************************************************\n";
        print "Cannot find '$msBuildPath'\n";
        print "Please make sure execute that the Microsoft .NET Framework SDK\n";
        print "is installed on this machine.\n";
        print "*************************************************************\n";
        die;
    }
}

sub buildXCodeProject($$@)
{
    my ($project, $clean, @extraOptions) = @_;

    if ($clean) {
        push(@extraOptions, "-alltargets");
        push(@extraOptions, "clean");
    }

    chomp($ENV{DSYMUTIL_NUM_THREADS} = `sysctl -n hw.activecpu`);

    # lldbWebKitTester won't work with the wrong CLANG_DEBUG_INFORMATION_LEVEL, so always use the default for that project
    if ($project eq "lldbWebKitTester") {
        my $index = 0;
        while ($index < scalar(@extraOptions)) {
            if ($extraOptions[$index] =~ /CLANG_DEBUG_INFORMATION_LEVEL=/) {
                splice @extraOptions, $index, 1;
            } else {
                $index += 1;
            }
        }
    }
    return system "xcodebuild", "-project", "$project.xcodeproj", @extraOptions;
}

sub getVisualStudioToolset()
{
    if (isPlayStation()) {
        return "";
    } elsif (isWin64()) {
        return "x64";
    } else {
        return "Win32";
    }
}

sub getMSBuildPlatformArgument()
{
    my $toolset = getVisualStudioToolset();
    if (defined($toolset) && length($toolset)) {
        return "/p:Platform=$toolset";
    }
    return "";
}

sub getCMakeWindowsToolsetArgument()
{
    my $toolset = getVisualStudioToolset();
    if (defined($toolset) && length($toolset)) {
        return "-A $toolset";
    }
    return "";
}

sub buildVisualStudioProject
{
    my ($project, $clean) = @_;
    setupCygwinEnv();

    my $config = configurationForVisualStudio();

    chomp($project = `cygpath -w "$project"`) if isCygwin();

    my $action = "/t:build";
    if ($clean) {
        $action = "/t:clean";
    }

    my $platform = getMSBuildPlatformArgument();
    my $logPath = File::Spec->catdir($baseProductDir, $configuration);
    make_path($logPath) unless -d $logPath or $logPath eq ".";

    my $errorLogFile = File::Spec->catfile($logPath, "webkit_errors.log");
    chomp($errorLogFile = `cygpath -w "$errorLogFile"`) if isCygwin();
    my $errorLogging = "/flp:LogFile=" . $errorLogFile . ";ErrorsOnly";

    my $warningLogFile = File::Spec->catfile($logPath, "webkit_warnings.log");
    chomp($warningLogFile = `cygpath -w "$warningLogFile"`) if isCygwin();
    my $warningLogging = "/flp1:LogFile=" . $warningLogFile . ";WarningsOnly";

    my $maxCPUCount = '/maxcpucount:' . numberOfCPUs();

    my @command = ($msBuildPath, "/verbosity:minimal", $project, $action, $config, $platform, "/fl", $errorLogging, "/fl1", $warningLogging, $maxCPUCount);
    print join(" ", @command), "\n";
    return system @command;
}

sub getJhbuildPath()
{
    my @jhbuildPath = File::Spec->splitdir(baseProductDir());
    if (isGit() && isGitBranchBuild() && gitBranch()) {
        pop(@jhbuildPath);
    }
    if (isGtk()) {
        push(@jhbuildPath, "DependenciesGTK");
    } elsif (isWPE()) {
        push(@jhbuildPath, "DependenciesWPE");
    } else {
        die "Cannot get JHBuild path for platform that isn't GTK+ or WPE.\n";
    }
    return File::Spec->catdir(@jhbuildPath);
}

sub getUserFlatpakPath()
{
    my $productDir = baseProductDir();
    if (isGit() && isGitBranchBuild() && gitBranch()) {
        my $branch = gitBranch();
        $productDir =~ s/$branch//;
    }
    my @flatpakPath = File::Spec->splitdir($productDir);
    push(@flatpakPath, "UserFlatpak");
    return File::Spec->catdir(@flatpakPath);
}

sub isCachedArgumentfileOutOfDate($@)
{
    my ($filename, $currentContents) = @_;

    if (! -e $filename) {
        return 1;
    }

    open(CONTENTS_FILE, $filename);
    chomp(my $previousContents = <CONTENTS_FILE> || "");
    close(CONTENTS_FILE);

    if ($previousContents ne $currentContents) {
        print "Contents for file $filename have changed.\n";
        print "Previous contents were: $previousContents\n\n";
        print "New contents are: $currentContents\n";
        return 1;
    }

    return 0;
}

sub inFlatpakSandbox()
{
    return (-f "/.flatpak-info");
}

sub runInFlatpak(@)
{
    if (isGtk() && checkForArgumentAndRemoveFromARGV("--update-gtk")) {
        system("perl", File::Spec->catfile(sourceDir(), "Tools", "Scripts", "update-webkitgtk-libs"), argumentsForConfiguration()) == 0 or die $!;
    }

    if (isWPE() && checkForArgumentAndRemoveFromARGV("--update-wpe")) {
        system("perl", File::Spec->catfile(sourceDir(), "Tools", "Scripts", "update-webkitwpe-libs"), argumentsForConfiguration()) == 0 or die $!;
    }

    my @arg = @_;
    my @command = (File::Spec->catfile(sourceDir(), "Tools", "Scripts", "webkit-flatpak"));

    my @flatpakArgs;
    my @filteredArgv;

    # Filter-out Flatpak SDK-specific arguments to a separate array, passed to webkit-flatpak.
    my $prefix = "--flatpak-";
    foreach my $opt (@ARGV) {
        if (substr($opt, 0, length($prefix)) eq $prefix) {
            my ($name, $value) = split("--flatpak-", $opt);
            push(@flatpakArgs, "--$value");
        } else {
            push(@filteredArgv, $opt);
        }
    }

    exec @command, argumentsForConfiguration(), @flatpakArgs, "--command", @_, argumentsForConfiguration(), @filteredArgv or die;
}

sub runInFlatpakIfAvailable(@)
{
    my $prefix = wrapperPrefixIfNeeded();
    if (defined($prefix)) {
        return 0;
    }

    if (inFlatpakSandbox()) {
        return 0;
    }

    my @command = (File::Spec->catfile(sourceDir(), "Tools", "Scripts", "webkit-flatpak"));
    if (system(@command, "--available") != 0) {
        return 0;
    }

    if (! -e getUserFlatpakPath()) {
      return 0;
    }

    runInFlatpak(@_)
}

sub jhbuildWrapperPrefix()
{
    my @prefix = (File::Spec->catfile(sourceDir(), "Tools", "jhbuild", "jhbuild-wrapper"));
    if (isGtk()) {
        push(@prefix, "--gtk");
    } elsif (isWPE()) {
        push(@prefix, "--wpe");
    }
    push(@prefix, "run");

    return @prefix;
}

sub wrapperPrefixIfNeeded()
{
    if (isAnyWindows() || isJSCOnly() || isPlayStation()) {
        return ();
    }
    if (isAppleCocoaWebKit()) {
        return ("xcrun");
    }

    # Returning () here means either Flatpak or no wrapper will be used.
    if (isGtk() or isWPE()) {
        # Respect user's choice.
        if (defined $ENV{'WEBKIT_JHBUILD'}) {
            if ($ENV{'WEBKIT_JHBUILD'} and -e getJhbuildPath()) {
                return jhbuildWrapperPrefix();
            } else {
                return ();
            }
            # or let Flatpak take precedence over JHBuild.
        } elsif (-e getUserFlatpakPath()) {
            return ();
        } elsif (-e getJhbuildPath()) {
            return jhbuildWrapperPrefix();
        }
    }
    return ();
}

sub shouldUseFlatpak()
{
    # TODO: Use flatpak for JSCOnly on Linux? Could be useful when the SDK
    # supports cross-compilation for ARMv7 and Aarch64 for instance.

    if (!isGtk() and !isWPE()) {
        return 0;
    }

    if (defined $ENV{'WEBKIT_JHBUILD'} && $ENV{'WEBKIT_JHBUILD'}) {
        return 0;
    }

    my @prefix = wrapperPrefixIfNeeded();
    return ((! inFlatpakSandbox()) and (@prefix == 0) and -e getUserFlatpakPath());
}

sub cmakeCachePath()
{
    return File::Spec->catdir(baseProductDir(), configuration(), "CMakeCache.txt");
}

sub cmakeFilesPath()
{
    return File::Spec->catdir(baseProductDir(), configuration(), "CMakeFiles");
}

sub shouldRemoveCMakeCache(@)
{
    my (@buildArgs) = @_;

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

    # FIXME: This probably does not work as expected, or the next block to
    # delete the images subdirectory would not be here. Directory mtime does not
    # percolate upwards when files are added or removed from subdirectories.
    my $inspectorUserInterfaceDirectory = File::Spec->catdir(sourceDir(), "Source", "WebInspectorUI", "UserInterface");
    if ($cacheFileModifiedTime < stat($inspectorUserInterfaceDirectory)->mtime) {
        return 1;
    }

    my $inspectorImageDirectory = File::Spec->catdir(sourceDir(), "Source", "WebInspectorUI", "UserInterface", "Images");
    if ($cacheFileModifiedTime < stat($inspectorImageDirectory)->mtime) {
        return 1;
    }

    if(isAnyWindows()) {
        my $winConfiguration = File::Spec->catdir(sourceDir(), "Source", "cmake", "OptionsWin.cmake");
        if ($cacheFileModifiedTime < stat($winConfiguration)->mtime) {
            return 1;
        }
    }

    # If a change on the JHBuild moduleset has been done, we need to clean the cache as well.
    if (! shouldUseFlatpak() and (isGtk() || isWPE())) {
        my $jhbuildRootDirectory = File::Spec->catdir(getJhbuildPath(), "Root");
        # The script update-webkit-libs-jhbuild shall re-generate $jhbuildRootDirectory if the moduleset changed.
        if (-d $jhbuildRootDirectory && $cacheFileModifiedTime < stat($jhbuildRootDirectory)->mtime) {
            return 1;
        }
    }

    return 0;
}

sub removeCMakeCache(@)
{
    my (@buildArgs) = @_;
    if (shouldRemoveCMakeCache(@buildArgs)) {
        my $cmakeCache = cmakeCachePath();
        my $cmakeFiles = cmakeFilesPath();
        unlink($cmakeCache) if -e $cmakeCache;
        rmtree($cmakeFiles) if -d $cmakeFiles;
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

    if (isAppleCocoaWebKit()) {
        my $devnull = File::Spec->devnull();
        if (exitStatus(system("xcrun -find ninja >$devnull 2>&1")) == 0) {
            return 1;
        }
    }

    # Test both ninja and ninja-build. Fedora uses ninja-build and has patched CMake to also call ninja-build.
    return commandExists("ninja") || commandExists("ninja-build");
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
    my $willUseNinja = canUseNinja();
    if (-e cmakeCachePath() && -e cmakeGeneratedBuildfile($willUseNinja)) {
        return 0;
    }

    my @args;
    push @args, "-DPORT=\"$port\"";
    push @args, "-DCMAKE_INSTALL_PREFIX=\"$prefixPath\"" if $prefixPath;
    push @args, "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON";
    if ($config =~ /release/i) {
        push @args, "-DCMAKE_BUILD_TYPE=Release";
    } elsif ($config =~ /debug/i) {
        push @args, "-DCMAKE_BUILD_TYPE=Debug";
    }

    push @args, "-DENABLE_SANITIZERS=address" if asanIsEnabled();

    push @args, "-DLTO_MODE=$ltoMode" if ltoMode();

    push @args, '-DCMAKE_TOOLCHAIN_FILE=Platform/PlayStation' if isPlayStation();

    if ($willUseNinja) {
        push @args, "-G";
        if (canUseEclipseNinjaGenerator()) {
            push @args, "'Eclipse CDT4 - Ninja'";
        } else {
            push @args, "Ninja";
        }
        push @args, "-DUSE_THIN_ARCHIVES=OFF" if isPlayStation();
    } else {
        if (isAnyWindows()) {
            push @args, getCMakeWindowsToolsetArgument();
        }
        if ((isAnyWindows() || isPlayStation()) && defined $ENV{VisualStudioVersion}) {
            my $var = int($ENV{VisualStudioVersion});
            push @args, qq(-G "Visual Studio $var");
        }
    }

    # Do not show progress of generating bindings in interactive Ninja build not to leave noisy lines on tty
    push @args, '-DSHOW_BINDINGS_GENERATION_PROGRESS=1' unless ($willUseNinja && -t STDOUT);

    # Some ports have production mode, but build-webkit should always use developer mode.
    push @args, "-DDEVELOPER_MODE=ON" if isGtk() || isJSCOnly() || isWPE() || isWin64();

    if (architecture() eq "x86_64" && shouldBuild32Bit()) {
        # CMAKE_LIBRARY_ARCHITECTURE is needed to get the right .pc
        # files in Debian-based systems, for the others
        # CMAKE_PREFIX_PATH will get us /usr/lib, which should be the
        # right path for 32bit. See FindPkgConfig.cmake.
        push @cmakeArgs, '-DFORCE_32BIT=ON -DCMAKE_PREFIX_PATH="/usr" -DCMAKE_LIBRARY_ARCHITECTURE=x86';
        $ENV{"CFLAGS"} =  "-m32" . ($ENV{"CFLAGS"} || "");
        $ENV{"CXXFLAGS"} = "-m32" . ($ENV{"CXXFLAGS"} || "");
        $ENV{"LDFLAGS"} = "-m32" . ($ENV{"LDFLAGS"} || "");
    }
    push @args, @cmakeArgs if @cmakeArgs;

    my $cmakeSourceDir = isCygwin() ? windowsSourceDir() : sourceDir();
    push @args, '"' . $cmakeSourceDir . '"';

    # We call system("cmake @args") instead of system("cmake", @args) so that @args is
    # parsed for shell metacharacters.
    my $wrapper = join(" ", wrapperPrefixIfNeeded()) . " ";
    my $returnCode = systemVerbose($wrapper . "cmake @args");

    chdir($originalWorkingDirectory);
    return $returnCode;
}

sub buildCMakeGeneratedProject($)
{
    my (@makeArgs) = @_;
    my $config = configuration();
    my $buildPath = File::Spec->catdir(baseProductDir(), $config);
    if (! -d $buildPath) {
        die "Must call generateBuildSystemFromCMakeProject() before building CMake project.";
    }

    if ($ENV{VERBOSE} && canUseNinja()) {
        push @makeArgs, "-v";
        push @makeArgs, "-d keeprsp" if (version->parse(determineNinjaVersion()) >= version->parse("1.4.0"));
    }

    my $command = "cmake";
    my @args = ("--build", $buildPath, "--config", $config);
    push @args, ("--", @makeArgs) if @makeArgs;

    # GTK and JSCOnly can use a build script to preserve colors and pretty-printing.
    if ((isGtk() || isJSCOnly()) && -e "$buildPath/build.sh") {
        chdir "$buildPath" or die;
        $command = "$buildPath/build.sh";
        @args = (@makeArgs);
    }

    # We call system("cmake @args") instead of system("cmake", @args) so that @args is
    # parsed for shell metacharacters. In particular, @makeArgs may contain such metacharacters.
    my $wrapper = join(" ", wrapperPrefixIfNeeded()) . " ";
    return systemVerbose($wrapper . "$command @args");
}

sub cleanCMakeGeneratedProject()
{
    my $config = configuration();
    my $buildPath = File::Spec->catdir(baseProductDir(), $config);
    if (-d $buildPath) {
        return systemVerbose("cmake", "--build", $buildPath, "--config", $config, "--target", "clean");
    }
    return 0;
}

sub buildCMakeProjectOrExit($$$@)
{
    my ($clean, $prefixPath, $makeArgs, @cmakeArgs) = @_;
    my $returnCode;

    exit(exitStatus(cleanCMakeGeneratedProject())) if $clean;

    my $wrapper = wrapperPrefixIfNeeded();
    my $jhbuildPrefix = jhbuildWrapperPrefix();
    if (defined($wrapper) && defined($jhbuildPrefix) && $wrapper == $jhbuildPrefix) {
        if (isGtk() && checkForArgumentAndRemoveFromARGV("--update-gtk")) {
            system("perl", File::Spec->catfile(sourceDir(), "Tools", "Scripts", "update-webkitgtk-libs")) == 0 or die $!;
        }

        if (isWPE() && checkForArgumentAndRemoveFromARGV("--update-wpe")) {
            system("perl", File::Spec->catfile(sourceDir(), "Tools", "Scripts", "update-webkitwpe-libs")) == 0 or die $!;
        }
    }

    $returnCode = exitStatus(generateBuildSystemFromCMakeProject($prefixPath, @cmakeArgs));
    exit($returnCode) if $returnCode;
    exit 0 if isGenerateProjectOnly();

    $returnCode = exitStatus(buildCMakeGeneratedProject($makeArgs));
    exit($returnCode) if $returnCode;
    return 0;
}

sub cmakeArgsFromFeatures(\@;$)
{
    my ($featuresArrayRef, $enableExperimentalFeatures) = @_;

    my @args;
    push @args, "-DENABLE_EXPERIMENTAL_FEATURES=ON" if $enableExperimentalFeatures;
    foreach (@$featuresArrayRef) {
        my $featureName = $_->{define};
        if ($featureName) {
            my $featureValue = ${$_->{value}}; # Undef to let the build system use its default.
            if (defined($featureValue)) {
                my $featureEnabled = $featureValue ? "ON" : "OFF";
                push @args, "-D$featureName=$featureEnabled";
            }
        }
    }
    return @args;
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
    return 1 unless isAppleCocoaWebKit();
    determineIsCMakeBuild();
    return $isCMakeBuild;
}

sub determineIsGenerateProjectOnly()
{
    return if defined($isGenerateProjectOnly);
    $isGenerateProjectOnly = checkForArgumentAndRemoveFromARGV("--generate-project-only");
}

sub isGenerateProjectOnly()
{
    determineIsGenerateProjectOnly();
    return $isGenerateProjectOnly;
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
        if (isAppleWinWebKit() || isFTW()) {
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

    print STDERR <<EOF;
Usage: @{[basename($0)]} [options] [args ...]
  --help                            Show this help message
  --no-saved-state                  Launch the application without state restoration

Options specific to macOS:
  -g|--guard-malloc                 Enable Guard Malloc
  --lang=LANGUAGE                   Use a specific language instead of system language.
                                    This accepts a language name (German) or a language code (de, ar, pt_BR, etc).
  --locale=LOCALE                   Use a specific locale instead of the system region.

Options specific to iOS:
  --iphone-simulator                Run the app in an iPhone Simulator
  --ipad-simulator                  Run the app in an iPad Simulator
EOF

    exit(1);
}

sub argumentsForRunAndDebugMacWebKitApp()
{
    my @args = ();
    if (checkForArgumentAndRemoveFromARGV("--no-saved-state")) {
        push @args, ("-ApplePersistenceIgnoreStateQuietly", "YES");
    }

    my $lang;
    if (checkForArgumentAndRemoveFromARGVGettingValue("--lang", \$lang)) {
        push @args, ("-AppleLanguages", "(" . $lang . ")");
    }

    my $locale;
    if (checkForArgumentAndRemoveFromARGVGettingValue("--locale", \$locale)) {
        push @args, ("-AppleLocale", $locale);
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
    prependToEnvironmentVariableList("DYLD_LIBRARY_PATH", $dyldFrameworkPath);
    prependToEnvironmentVariableList("__XPC_DYLD_LIBRARY_PATH", $dyldFrameworkPath);
    $ENV{WEBKIT_UNSET_DYLD_FRAMEWORK_PATH} = "YES";

    setUpGuardMallocIfNeeded();
}

sub setupUnixWebKitEnvironment($)
{
    my ($productDir) = @_;

    $ENV{TEST_RUNNER_INJECTED_BUNDLE_FILENAME} = File::Spec->catfile($productDir, "lib", "libTestRunnerInjectedBundle.so");
    $ENV{TEST_RUNNER_TEST_PLUGIN_PATH} = File::Spec->catdir($productDir, "lib", "plugins");
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
    my $output = `xcrun simctl list runtimes iOS --json`;
    my $runtimes = decode_json($output)->{runtimes};
    if (!$runtimes) {
        die "No iOS simulator runtimes found";
    }
    my $runtimePath = @$runtimes[0]->{runtimeRoot};
    return File::Spec->catdir($runtimePath, "Applications");
}

sub installedMobileSafariBundle()
{
    return File::Spec->catfile(iosSimulatorApplicationsPath(), "MobileSafari.app");
}

sub mobileSafariBundle()
{
    determineConfigurationProductDir();

    # Use MobileSafari.app in product directory if present.
    if (isIOSWebKit() && -d "$configurationProductDir/MobileSafari.app") {
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
    # FIXME: We should add a maximum time limit to wait here.
    while ($device->{state} ne $waitUntilState) {
        usleep(500 * 1000); # Waiting 500ms between file system polls does not make script run-safari feel sluggish.
        $device = iosSimulatorDeviceByUDID($deviceUDID);
    }
}

sub waitUntilProcessNotRunning($)
{
    my ($process) = @_;
    while (system("/bin/ps -eo pid,comm | /usr/bin/grep '$process\$'") == 0) {
        usleep(500 * 1000);
    }
}

sub shutDownIOSSimulatorDevice($)
{
    my ($simulatorDevice) = @_;

    return if $simulatorDevice->{state} eq SIMULATOR_DEVICE_STATE_SHUTDOWN;
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
    shutDownIOSSimulatorDevice($simulatedDevice);

    chomp(my $developerDirectory = $ENV{DEVELOPER_DIR} || `xcode-select --print-path`); 
    my $iosSimulatorPath = File::Spec->catfile($developerDirectory, "Applications", "Simulator.app");
    # Simulator.app needs to be running before the simulator is booted to have it visible.
    system("open", "-a", $iosSimulatorPath, "--args", "-CurrentDeviceUDID", $simulatedDevice->{UDID}) == 0 or die "Failed to open $iosSimulatorPath: $!"; 
    system("xcrun", "simctl", "boot", $simulatedDevice->{UDID}) == 0 or die "Failed to boot simulator $simulatedDevice->{UDID}: $!"; 

    waitUntilIOSSimulatorDeviceIsInState($simulatedDevice->{UDID}, SIMULATOR_DEVICE_STATE_BOOTED);
    waitUntilProcessNotRunning("com.apple.datamigrator");
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

    my $output = `xcrun simctl list devices $simulatedDeviceUDID --json`;
    my $runtimes = decode_json($output)->{devices};

    while ((my $runtime, my $devicesForRuntime) = each %$runtimes) {
        next if not @$devicesForRuntime;
        die "Multiple devices found for UDID $simulatedDeviceUDID: $output" if scalar(@$devicesForRuntime) > 1;
        return simulatorDeviceFromJSON($runtime, @$devicesForRuntime[0]);        
    }
    return undef;
}

sub iosSimulatorRuntime()
{
    my $xcodeSDKVersion = xcodeSDKVersion();
    $xcodeSDKVersion =~ s/\./-/;
    my $runtime = "com.apple.CoreSimulator.SimRuntime.iOS-$xcodeSDKVersion";

    open(TEST, "-|", "xcrun --sdk iphonesimulator simctl list 2>&1") or die "Failed to run find simulator runtime";
    while ( my $line = <TEST> ) {
        $runtime = $1 if ($line =~ m/.+ - (com.apple.CoreSimulator.SimRuntime.iOS-\S+)/);
    }
    close(TEST);
    return $runtime;
}

sub findOrCreateSimulatorForIOSDevice($)
{
    my ($simulatorNameSuffix) = @_;
    my $simulatorName;
    my $simulatorDeviceType;

    if ($simulatorIdiom eq "iPad") {
        $simulatorName = "iPad Pro " . $simulatorNameSuffix;
        $simulatorDeviceType = "com.apple.CoreSimulator.SimDeviceType.iPad-Pro--9-7-inch-";
    } else {
        $simulatorName = "iPhone SE " . $simulatorNameSuffix;
        $simulatorDeviceType = "com.apple.CoreSimulator.SimDeviceType.iPhone-SE";
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
            shutDownIOSSimulatorDevice($simulatedDevice);
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

sub archCommandLineArgumentsForRestrictedEnvironmentVariables()
{
    my @arguments = ();
    foreach my $key (keys(%ENV)) {
        if ($key =~ /^DYLD_/) {
            push @arguments, "-e", "$key=$ENV{$key}";
        }
    }
    return @arguments;
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
        return system "arch", "-" . architecture(), archCommandLineArgumentsForRestrictedEnvironmentVariables(), $appPath, argumentsForRunAndDebugMacWebKitApp();
    }
    return system { $appPath } $appPath, argumentsForRunAndDebugMacWebKitApp();
}

sub execMacWebKitAppForDebugging($)
{
    my ($appPath) = @_;
    my $architectureSwitch = "--arch";
    my $argumentsSeparator = "--";

    my $debuggerPath = `xcrun -find lldb`;
    chomp $debuggerPath;
    die "Can't find the lldb executable.\n" unless -x $debuggerPath;

    my $productDir = productDir();
    setupMacWebKitEnvironment($productDir);

    my @architectureFlags = ($architectureSwitch, architecture());
    print "Starting @{[basename($appPath)]} under lldb with DYLD_FRAMEWORK_PATH set to point to built WebKit in $productDir.\n";
    exec { $debuggerPath } $debuggerPath, @architectureFlags, $argumentsSeparator, $appPath, argumentsForRunAndDebugMacWebKitApp() or die;
}

sub execUnixAppForDebugging($)
{
    my ($appPath) = @_;

    my $debuggerPath = `which gdb | head -1`;
    chomp $debuggerPath;
    die "Can't find the gdb executable.\n" unless -x $debuggerPath;

    my $productDir = productDir();
    setupUnixWebKitEnvironment($productDir);

    my @cmdline = wrapperPrefixIfNeeded();
    push @cmdline, $debuggerPath, "--args", $appPath;

    print "Starting @{[basename($appPath)]} under gdb with build WebKit in $productDir.\n";
    exec @cmdline, @ARGV or die;
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

    if (isAppleWinWebKit() || isFTW()) {
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
    }
    if (isAppleWinWebKit() || isFTW()) {
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
    } elsif (isGtk() or isWPE()) {
        execUnixAppForDebugging(File::Spec->catfile(productDir(), "bin", "WebKitTestRunner"));
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
