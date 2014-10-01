# Copyright (C) 2005-2007, 2010-2014 Apple Inc. All rights reserved.
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
use Digest::MD5 qw(md5_hex);
use FindBin;
use File::Basename;
use File::Path qw(mkpath rmtree);
use File::Spec;
use File::stat;
use List::Util;
use POSIX;
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
       &baseProductDir
       &chdirWebKit
       &checkFrameworks
       &cmakeBasedPortArguments
       &cmakeBasedPortName
       &currentSVNRevision
       &debugSafari
       &findOrCreateSimulatorForIOSDevice
       &installAndLaunchIOSWebKitAppInSimulator
       &iosSimulatorDeviceByName
       &nmPath
       &openIOSSimulator
       &passedConfiguration
       &printHelpAndExitForRunAndDebugWebKitAppIfNeeded
       &productDir
       &quitIOSSimulator
       &runIOSWebKitApp
       &runMacWebKitApp
       &safariPath
       &setConfiguration
       &setupMacWebKitEnvironment
       &sharedCommandLineOptions
       &sharedCommandLineOptionsUsage
       USE_OPEN_COMMAND
   );
   %EXPORT_TAGS = ( );
   @EXPORT_OK   = ();
}

use constant USE_OPEN_COMMAND => 1; # Used in runMacWebKitApp().
use constant INCLUDE_OPTIONS_FOR_DEBUGGING => 1;

our @EXPORT_OK;

my $architecture;
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
my $generateDsym;
my $isGtk;
my $isWinCE;
my $isWinCairo;
my $isWin64;
my $isEfl;
my $isInspectorFrontend;
my $isWK2;
my $shouldTargetWebProcess;
my $shouldUseXPCServiceForWebProcess;
my $shouldUseGuardMalloc;
my $xcodeVersion;

# Variables for Win32 support
my $programFilesPath;
my $vcBuildPath;
my $vsInstallDir;
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
    until ((-d "$sourceDir/Source" && -d "$sourceDir/Source/WebCore" && -d "$sourceDir/Source/WebKit") || (-d "$sourceDir/Internal" && -d "$sourceDir/OpenSource"))
    {
        if ($sourceDir !~ s|/[^/]+$||) {
            die "Could not find top level webkit directory above source directory using FindBin.\n";
        }
    }

    $sourceDir = "$sourceDir/OpenSource" if -d "$sourceDir/OpenSource";
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
        $baseProductDir = "$sourceDir/WebKitBuild";
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
        $baseProductDir = $unixBuildPath;
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

    if ($configuration && isWinCairo()) {
        unless ($configuration =~ /_WinCairo$/) {
            $configuration .= "_WinCairo";
        }
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
    } elsif (isEfl() || isGtk()) {
        my $host_processor = "";
        $host_processor = `cmake --system-information | grep CMAKE_SYSTEM_PROCESSOR`;
        if ($host_processor =~ m/^CMAKE_SYSTEM_PROCESSOR \"([^"]+)\"/) {
            # We have a configured build tree; use it.
            $architecture = $1;
            $architecture = 'x86_64' if $architecture eq 'amd64';
        }
    }

    if (!$architecture && (isGtk() || isAppleMacWebKit() || isEfl())) {
        # Fall back to output of `arch', if it is present.
        $architecture = `arch`;
        chomp $architecture;
    }

    if (!$architecture && (isGtk() || isAppleMacWebKit() || isEfl())) {
        # Fall back to output of `uname -m', if it is present.
        $architecture = `uname -m`;
        chomp $architecture;
    }

    $architecture = 'x86_64' if ($architecture =~ /amd64/ && isBSD());
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
    } elsif (isWindows() || isCygwin()) {
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
    $jscName .= ".exe" if (isWindows() || isCygwin());
    return "$productDir/$jscName" if -e "$productDir/$jscName";
    return "$productDir/JavaScriptCore.framework/Resources/$jscName";
}

sub argumentsForConfiguration()
{
    determineConfiguration();
    determineArchitecture();
    determineXcodeSDK();

    my @args = ();
    push(@args, '--debug') if ($configuration =~ "^Debug");
    push(@args, '--release') if ($configuration =~ "^Release");
    push(@args, '--device') if (defined $xcodeSDK && $xcodeSDK =~ /^iphoneos/);
    push(@args, '--sim') if (defined $xcodeSDK && $xcodeSDK =~ /^iphonesimulator/);
    push(@args, '--ios-simulator') if (defined $xcodeSDK && $xcodeSDK =~ /^iphonesimulator/);
    push(@args, '--32-bit') if ($architecture ne "x86_64" and !isWin64());
    push(@args, '--64-bit') if (isWin64());
    push(@args, '--gtk') if isGtk();
    push(@args, '--efl') if isEfl();
    push(@args, '--wincairo') if isWinCairo();
    push(@args, '--wince') if isWinCE();
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
        $xcodeSDK ||= 'iphoneos.internal';
    }
    if (checkForArgumentAndRemoveFromARGV("--sim") ||
        checkForArgumentAndRemoveFromARGV("--simulator") ||
        checkForArgumentAndRemoveFromARGV("--ios-simulator")) {
        $xcodeSDK ||= 'iphonesimulator';
    }
}

sub xcodeSDK
{
    determineXcodeSDK();
    return $xcodeSDK;
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
        $vsInstallDir = File::Spec->catdir(programFilesPath(), "Microsoft Visual Studio 12.0");
    }
    chomp($vsInstallDir = `cygpath "$vsInstallDir"`) if isCygwin();

    return $vsInstallDir;
}

sub visualStudioVersion
{
    return $vsVersion if defined $vsVersion;

    my $installDir = visualStudioInstallDir();

    $vsVersion = ($installDir =~ /Microsoft Visual Studio ([0-9]+\.[0-9]*)/) ? $1 : "12";

    return $vsVersion;
}

sub determineConfigurationForVisualStudio
{
    return if defined $configurationForVisualStudio;
    determineConfiguration();
    # FIXME: We should detect when Debug_All or Production has been chosen.
    $configurationForVisualStudio = $configuration . (isWin64() ? "|x64" : "|Win32");
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
        my $binDir = isWin64() ? "bin64" : "bin32";
        $configurationProductDir = File::Spec->catdir($baseProductDir, $configuration, $binDir);
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

sub jscProductDir
{
    my $productDir = productDir();
    $productDir .= "/bin" if (isEfl() || isGtk());

    return $productDir;
}

sub configuration()
{
    determineConfiguration();
    return $configuration;
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
    determineXcodeSDK();

    my @sdkOption = ($xcodeSDK ? "SDKROOT=$xcodeSDK" : ());
    my @architectureOption = ($architecture ? "ARCHS=$architecture" : ());

    return (@baseProductDirOption, "-configuration", $configuration, @architectureOption, @sdkOption, argumentsForXcode());
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

    $passedConfiguration .= "_WinCairo" if (defined($passedConfiguration) && isWinCairo() && isCygwin());
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
    if (isWinCE()) {
        return "$configurationProductDir/$libraryName";
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
    return if isCygwin() || isWindows();
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
    return `$command --version 2> $devnull`;
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

sub isWK2()
{
    if (defined($isWK2)) {
        return $isWK2;
    }
    if (checkForArgumentAndRemoveFromARGV("-2")) {
        $isWK2 = 1;
    } else {
        $isWK2 = 0;
    }
    return $isWK2;
}

sub determineIsEfl()
{
    return if defined($isEfl);
    $isEfl = checkForArgumentAndRemoveFromARGV("--efl");
}

sub isEfl()
{
    determineIsEfl();
    return $isEfl;
}

sub determineIsGtk()
{
    return if defined($isGtk);
    $isGtk = checkForArgumentAndRemoveFromARGV("--gtk");
}

sub isGtk()
{
    determineIsGtk();
    return $isGtk;
}

sub isWinCE()
{
    determineIsWinCE();
    return $isWinCE;
}

sub determineIsWinCE()
{
    return if defined($isWinCE);
    $isWinCE = checkForArgumentAndRemoveFromARGV("--wince");
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
    determineIsWinCairo();
    return $isWinCairo;
}

sub determineIsWinCairo()
{
    return if defined($isWinCairo);
    $isWinCairo = checkForArgumentAndRemoveFromARGV("--wincairo");
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
    return isDarwin() && !isGtk();
}

sub isAppleWinWebKit()
{
    return (isCygwin() || isWindows()) && !isWinCairo() && !isGtk() && !isWinCE();
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

sub deleteiOSSimulatorDevice
{
    my $udid = shift;
    return system("xcrun", "--sdk", "iphonesimulator", "simctl", "delete", $udid);
}

sub willUseIOSDeviceSDKWhenBuilding()
{
    return xcodeSDKPlatformName() eq "iphoneos";
}

sub willUseIOSSimulatorSDKWhenBuilding()
{
    return xcodeSDKPlatformName() eq "iphonesimulator";
}

sub isIOSWebKit()
{
    determineXcodeSDK();
    return isAppleMacWebKit() && (willUseIOSDeviceSDKWhenBuilding() || willUseIOSSimulatorSDKWhenBuilding());
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

sub determineOSXVersion()
{
    return if $osXVersion;

    if (!isDarwin()) {
        $osXVersion = -1;
        return;
    }

    my $version = `sw_vers -productVersion`;
    my @splitVersion = split(/\./, $version);
    @splitVersion >= 2 or die "Invalid version $version";
    $osXVersion = {
            "major" => $splitVersion[0],
            "minor" => $splitVersion[1],
            "subminor" => (defined($splitVersion[2]) ? $splitVersion[2] : 0),
    };
}

sub osXVersion()
{
    determineOSXVersion();
    return $osXVersion;
}

sub isWindowsNT()
{
    return $ENV{'OS'} eq 'Windows_NT';
}

sub shouldTargetWebProcess
{
    determineShouldTargetWebProcess();
    return $shouldTargetWebProcess;
}

sub determineShouldTargetWebProcess
{
    return if defined($shouldTargetWebProcess);
    $shouldTargetWebProcess = checkForArgumentAndRemoveFromARGV("--target-web-process");
}

sub shouldUseXPCServiceForWebProcess
{
    determineShouldUseXPCServiceForWebProcess();
    return $shouldUseXPCServiceForWebProcess;
}

sub determineShouldUseXPCServiceForWebProcess
{
    return if defined($shouldUseXPCServiceForWebProcess);
    $shouldUseXPCServiceForWebProcess = checkForArgumentAndRemoveFromARGV("--use-web-process-xpc-service");
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

sub appendToEnvironmentVariableList
{
    my ($environmentVariableName, $value) = @_;

    if (defined($ENV{$environmentVariableName})) {
        $ENV{$environmentVariableName} .= ":" . $value;
    } else {
        $ENV{$environmentVariableName} = $value;
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
    if (isGtk() || isEfl() || isWinCE()) {
        return "$relativeScriptsPath/run-launcher";
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
        return "WinLauncher";
    } elsif (isWinCE()) {
        return "WinCELauncher";
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
    } elsif (isGtk() or isEfl() or isWindows()) {
        my @cmds = qw(bison gperf flex);
        my @missing = ();
        my $oldPath = $ENV{PATH};
        foreach my $cmd (@cmds) {
            push @missing, $cmd if not commandExists($cmd);
        }

        if (@missing) {
            my $list = join ", ", @missing;
            die "ERROR: $list missing but required to build WebKit.\n";
        }
    }
    # Win32 and other platforms may want to check for minimum config
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
    return windowsSourceDir() . "\\Source";
}

sub windowsLibrariesDir()
{
    return windowsSourceDir() . "\\WebKitLibraries\\win";
}

sub windowsOutputDir()
{
    return windowsSourceDir() . "\\WebKitBuild";
}

sub fontExists($)
{
    my $font = shift;
    my $val = system qw(regtool get), '\\HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts\\' . $font . ' (TrueType)';
    return 0 == $val;
}

sub checkInstalledTools()
{
    # SVN 1.7.10 is known to be compatible with current servers. SVN 1.8.x seems to be missing some authentication
    # protocols we use for svn.webkit.org:
    my $svnVersion = `svn --version | grep "\\sversion"`;
    chomp($svnVersion);
    if (!$? and $svnVersion =~ /1\.8\./) {
        print "svn 1.7.10 is known to be compatible with our servers. You are running $svnVersion,\nwhich may not work properly.\n"
    }

    # environment variables. Avoid until this is corrected.
    my $pythonVer = `python --version 2>&1`;
    die "You must have Python installed to build WebKit.\n" if ($?);

    # cURL 7.34.0 has a bug that prevents authentication with opensource.apple.com (and other things using SSL3).
    my $curlVer = `curl --version | grep "curl"`;
    chomp($curlVer);
    if (!$? and $curlVer =~ /libcurl\/7\.34\.0/) {
        print "cURL version 7.34.0 has a bug that prevents authentication with SSL v2 or v3.\n";
        print "cURL 7.33.0 is known to work. The cURL projects is preparing an update to\n";
        print "correct this problem.\n\n";
        die "Please install a working cURL and try again.\n";
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
        if (version->parse($uname_version) < version->parse("1.7.10")) {
            # Setting the environment variable 'CYGWIN' to 'tty' makes cygwin enable extra support (i.e., termios)
            # for UNIX-like ttys in the Windows console
            $variablesToSet{CYGWIN} = "tty" unless $ENV{CYGWIN};
        }
        
        # Those environment variables must be set to be able to build inside Visual Studio.
        $variablesToSet{WEBKIT_LIBRARIES} = windowsLibrariesDir() unless $ENV{WEBKIT_LIBRARIES};
        $variablesToSet{WEBKIT_OUTPUTDIR} = windowsOutputDir() unless $ENV{WEBKIT_OUTPUTDIR};
        $variablesToSet{MSBUILDDISABLENODEREUSE} = "1" unless $ENV{MSBUILDDISABLENODEREUSE};

        foreach my $variable (keys %variablesToSet) {
            print "Setting the Environment Variable '" . $variable . "' to '" . $variablesToSet{$variable} . "'\n\n";
            system qw(regtool -s set), '\\HKEY_CURRENT_USER\\Environment\\' . $variable, $variablesToSet{$variable};
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
    return if !isCygwin() && !isWindows();
    return if $vcBuildPath;

    my $programFilesPath = programFilesPath();
    $vcBuildPath = File::Spec->catfile(visualStudioInstallDir(), qw(Common7 IDE devenv.com));
    if (-e $vcBuildPath) {
        # Visual Studio is installed;
        if (visualStudioVersion() eq "12") {
            $vcBuildPath = File::Spec->catfile(visualStudioInstallDir(), qw(Common7 IDE devenv.exe));
        }
    } else {
        # Visual Studio not found, try VC++ Express
        $vcBuildPath = File::Spec->catfile(visualStudioInstallDir(), qw(Common7 IDE WDExpress.exe));
        if (! -e $vcBuildPath) {
            print "*************************************************************\n";
            print "Cannot find '$vcBuildPath'\n";
            print "Please execute the file 'vcvars32.bat' from\n";
            print "'$programFilesPath\\Microsoft Visual Studio 12.0\\VC\\bin\\'\n";
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

    push(@extraOptions, ("-sdk", "iphonesimulator")) if willUseIOSSimulatorSDKWhenBuilding();
    push(@extraOptions, ("-sdk", "iphoneos.internal")) if willUseIOSDeviceSDKWhenBuilding();

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

    my $action = "/build";
    if ($clean) {
        $action = "/clean";
    }

    my @command = ($vcBuildPath, $project, $action, $config);

    print join(" ", @command), "\n";
    return system @command;
}

sub getJhbuildPath()
{
    my @jhbuildPath = File::Spec->splitdir(baseProductDir());
    if (isGit() && isGitBranchBuild() && gitBranch()) {
        pop(@jhbuildPath);
    }
    push(@jhbuildPath, "Dependencies");
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

sub jhbuildWrapperPrefixIfNeeded()
{
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
    if (isWinCE()) {
        return 0;
    }

    if (!isGtk()) {
        return 1;
    }

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
    # Test both ninja and ninja-build. Fedora uses ninja-build and has patched CMake to also call ninja-build.
    system('which ninja > /dev/null || which ninja-build > /dev/null');
    return $? == 0;
}

sub canUseEclipse(@)
{
    system('which eclipse > /dev/null');
    return $? == 0;
}

sub cmakeGeneratedBuildfile(@)
{
    my ($willUseNinja) = @_;
    if ($willUseNinja) {
        return File::Spec->catfile(baseProductDir(), configuration(), "build.ninja")
    } else {
        return File::Spec->catfile(baseProductDir(), configuration(), "Makefile")
    }
}

sub generateBuildSystemFromCMakeProject
{
    my ($port, $prefixPath, @cmakeArgs, $additionalCMakeArgs) = @_;
    my $config = configuration();
    my $buildPath = File::Spec->catdir(baseProductDir(), $config);
    File::Path::mkpath($buildPath) unless -d $buildPath;
    my $originalWorkingDirectory = getcwd();
    chdir($buildPath) or die;

    # For GTK+ we try to be smart about when to rerun cmake, so that we can have faster incremental builds.
    my $willUseNinja = isGtk() && canUseNinja();
    if (isGtk() && -e cmakeCachePath() && -e cmakeGeneratedBuildfile($willUseNinja)) {
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
        if (canUseEclipse()) {
            push @args, "'Eclipse CDT4 - Ninja'";
        } else {
            push @args, "Ninja";
        }
    }

    # GTK+ has a production mode, but build-webkit should always use developer mode.
    push @args, "-DDEVELOPER_MODE=ON" if isEfl() || isGtk();

    # Don't warn variables which aren't used by cmake ports.
    push @args, "--no-warn-unused-cli";
    push @args, @cmakeArgs if @cmakeArgs;
    push @args, $additionalCMakeArgs if $additionalCMakeArgs;

    push @args, '"' . sourceDir() . '"';

    # Compiler options to keep floating point values consistent
    # between 32-bit and 64-bit architectures.
    determineArchitecture();
    if ($architecture ne "x86_64" && !isARM() && !isCrossCompilation()) {
        $ENV{'CXXFLAGS'} = "-march=pentium4 -msse2 -mfpmath=sse " . ($ENV{'CXXFLAGS'} || "");
    }

    # We call system("cmake @args") instead of system("cmake", @args) so that @args is
    # parsed for shell metacharacters.
    my $wrapper = join(" ", jhbuildWrapperPrefixIfNeeded()) . " ";
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

    # We call system("cmake @args") instead of system("cmake", @args) so that @args is
    # parsed for shell metacharacters. In particular, $makeArgs may contain such metacharacters.
    my $wrapper = join(" ", jhbuildWrapperPrefixIfNeeded()) . " ";
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

sub buildCMakeProjectOrExit($$$$@)
{
    my ($clean, $port, $prefixPath, $makeArgs, @cmakeArgs) = @_;
    my $returnCode;

    exit(exitStatus(cleanCMakeGeneratedProject())) if $clean;

    if (isEfl() && checkForArgumentAndRemoveFromARGV("--update-efl")) {
        system("perl", "$sourceDir/Tools/Scripts/update-webkitefl-libs") == 0 or die $!;
    }

    if (isGtk() && checkForArgumentAndRemoveFromARGV("--update-gtk")) {
        system("perl", "$sourceDir/Tools/Scripts/update-webkitgtk-libs") == 0 or die $!;
    }

    $returnCode = exitStatus(generateBuildSystemFromCMakeProject($port, $prefixPath, @cmakeArgs));
    exit($returnCode) if $returnCode;

    $returnCode = exitStatus(buildCMakeGeneratedProject($makeArgs));
    exit($returnCode) if $returnCode;
    return 0;
}

sub cmakeBasedPortArguments()
{
    return ('-G "Visual Studio 8 2005 STANDARDSDK_500 (ARMV4I)"') if isWinCE();
    return ();
}

sub cmakeBasedPortName()
{
    return "Efl" if isEfl();
    return "WinCE" if isWinCE();
    return "GTK" if isGtk();
    return "";
}

sub isCMakeBuild()
{
    return isEfl() || isWinCE() || isGtk();
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

    if (isAppleWinWebKit()) {
        $env->{PATH} = join(':', productDir(), appleApplicationSupportPath(), $env->{PATH} || "");
    } elsif (isWinCairo()) {
        my $winCairoBin = sourceDir() . "/WebKitLibraries/win/" . (isWin64() ? "bin64/" : "bin32/");
        my $gstreamerBin = isWin64() ? $ENV{"GSTREAMER_1_0_ROOT_X86_64"} . "bin" : $ENV{"GSTREAMER_1_0_ROOT_X86"} . "bin";
        $env->{PATH} = join(':', productDir(), $winCairoBin, $gstreamerBin, $env->{PATH} || "");
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
  --use-web-process-xpc-service     Launch the Web Process as an XPC Service (OS X only)
EOF

    if ($includeOptionsForDebugging) {
        print STDERR <<EOF;
  --target-web-process              Debug the web process
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
    push @args, ("-WebKit2UseXPCServiceForWebProcess", "YES") if shouldUseXPCServiceForWebProcess();
    unshift @args, @ARGV;

    return @args;
}

sub setupMacWebKitEnvironment($)
{
    my ($dyldFrameworkPath) = @_;

    $dyldFrameworkPath = File::Spec->rel2abs($dyldFrameworkPath);

    $ENV{DYLD_FRAMEWORK_PATH} = $ENV{DYLD_FRAMEWORK_PATH} ? join(":", $dyldFrameworkPath, $ENV{DYLD_FRAMEWORK_PATH}) : $dyldFrameworkPath;
    $ENV{__XPC_DYLD_FRAMEWORK_PATH} = $dyldFrameworkPath;
    $ENV{WEBKIT_UNSET_DYLD_FRAMEWORK_PATH} = "YES";

    setUpGuardMallocIfNeeded();
}

sub setupIOSWebKitEnvironment($)
{
    my ($dyldFrameworkPath) = @_;
    $dyldFrameworkPath = File::Spec->rel2abs($dyldFrameworkPath);

    $ENV{DYLD_FRAMEWORK_PATH} = $dyldFrameworkPath;
    $ENV{DYLD_LIBRARY_PATH} = $dyldFrameworkPath;

    setUpGuardMallocIfNeeded();
}

sub installedMobileSafariBundle()
{
    return File::Spec->catfile(XcodeSDKPath(), "Applications", "MobileSafari.app");
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
    return "$appBundle/Contents/Info.plist" if "$appBundle/Contents/Info.plist"; # Mac app bundle
    return "";
}

sub appIdentiferFromBundle($)
{
    my ($appBundle) = @_;
    my $plistPath = plistPathFromBundle($appBundle);
    chomp(my $bundleIdentifer = `defaults read '$plistPath' CFBundleIdentifier 2> /dev/null`);
    return $bundleIdentifer;
}

sub appDisplayNameFromBundle($)
{
    my ($appBundle) = @_;
    my $plistPath = plistPathFromBundle($appBundle);
    chomp(my $bundleDisplayName = `defaults read '$plistPath' CFBundleDisplayName 2> /dev/null`);
    return $bundleDisplayName;
}

sub loadIPhoneSimulatorNotificationIfNeeded()
{
    return if $didLoadIPhoneSimulatorNotification;
    push(@INC, productDir() . "/lib/perl5/darwin-thread-multi-2level");
    require IPhoneSimulatorNotification;
    $didLoadIPhoneSimulatorNotification = 1;
}

sub openIOSSimulator()
{
    chomp(my $developerDirectory = $ENV{DEVELOPER_DIR} || `xcode-select --print-path`);
    my $iosSimulatorPath = File::Spec->catfile($developerDirectory, "Applications", "iOS Simulator.app");

    loadIPhoneSimulatorNotificationIfNeeded();

    my $iPhoneSimulatorNotification = new IPhoneSimulatorNotification;
    $iPhoneSimulatorNotification->startObservingReadyNotification();
    system("open", "-a", $iosSimulatorPath, "--args", "-SessionOnLaunch", "NO") == 0 or die "Failed to open $iosSimulatorPath: $!";
    while (!$iPhoneSimulatorNotification->hasReceivedReadyNotification()) {
        my $date = NSDate->alloc()->initWithTimeIntervalSinceNow_(0.1);
        NSRunLoop->currentRunLoop->runUntilDate_($date);
        $date->release();
    }
    $iPhoneSimulatorNotification->stopObservingReadyNotification();
}

sub iosSimulatorDeviceByName($)
{
    my ($simulatorName) = @_;
    my @devices = grep {$_->{name} eq $simulatorName} iOSSimulatorDevices();
    my $deviceToUse = $devices[0];
    if (@devices > 1) {
        print "Warning: Found more than one simulator device named '$simulatorName'.\n";
        print "         Using simulator device with UDID: $deviceToUse->{UDID}.\n";
        print "         To see the list of simulator devices, run:\n";
        print "         xcrun --sdk iphonesimulator simctl list\n";
    }
    return $deviceToUse;
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

sub runIOSWebKitAppInSimulator($;$)
{
    my ($appBundle, $simulatorOptions) = @_;
    my $productDir = productDir();
    my $appDisplayName = appDisplayNameFromBundle($appBundle);
    print "Starting $appDisplayName with DYLD_FRAMEWORK_PATH set to point to built WebKit in $productDir.\n";

    $simulatorOptions = {} unless $simulatorOptions;

    my %simulatorENV;
    %simulatorENV = %{$simulatorOptions->{applicationEnvironment}} if $simulatorOptions->{applicationEnvironment};
    {
        local %ENV; # Shadow global-scope %ENV so that changes to it will not be seen outside of this scope.
        setupIOSWebKitEnvironment($productDir);
        %simulatorENV = %ENV;
    }
    $simulatorOptions->{applicationEnvironment} = \%simulatorENV;
    return installAndLaunchIOSWebKitAppInSimulator($appBundle, findOrCreateSimulatorForIOSDevice("For WebKit Development"), $simulatorOptions) <= 0;
}

# Launches the iOS WebKit-based application in the specified simulator device and dynamically
# linked against the built WebKit. The application will be installed if applicable.
#
# Args:
#   $appBundle: the path to the app bundle to launch.
#   $simulatedDevice: the simulator device to use to run the app.
#   $simulatorOptions: a hash reference representing optional simulator options.
#     sessionUUID: a unique identifer to use for the iOS Simulator session. Defaults to an identifer
#                  of the form "theAwesomeUniqueSessionIdentifierForX" where X is the display name of
#                  the specified app.
#     applicationArguments: an array reference representing the arguments to pass to the app (defaults to \@ARGV).
#     applicationEnvironment: a hash reference representing the environment variables to use when launching the app (defaults to {}).
#
# Returns the process identifier of the launched app.
sub installAndLaunchIOSWebKitAppInSimulator($$;$)
{
    my ($appBundle, $simulatedDevice, $simulatorOptions) = @_;

    loadIPhoneSimulatorNotificationIfNeeded();

    my $makeNSDictionaryFromHash = sub {
        my ($dict) = @_;
        my $result = NSMutableDictionary->alloc()->initWithCapacity_(scalar(keys %{$dict}));
        for my $key (keys %{$dict}) {
            $result->setObject_forKey_(NSString->stringWithCString_($dict->{$key}), NSString->stringWithCString_($key));
        }
        return $result->autorelease();
    };
    my $makeNSArrayFromArray = sub {
        my ($array) = @_;
        my $result = NSMutableArray->alloc()->initWithCapacity_(scalar(@{$array}));
        for my $item (@{$array}) {
            $result->addObject_(NSString->stringWithCString_($item));
        }
        return $result->autorelease();
    };

    my $simulatorENVHashRef = {};
    $simulatorENVHashRef = $simulatorOptions->{applicationEnvironment} if $simulatorOptions && $simulatorOptions->{applicationEnvironment};
    my $applicationArguments = \@ARGV;
    $applicationArguments = $simulatorOptions->{applicationArguments} if $simulatorOptions && $simulatorOptions->{applicationArguments};
    my $sessionUUID;
    if ($simulatorOptions && $simulatorOptions->{sessionUUID}) {
        $sessionUUID = $simulatorOptions->{sessionUUID};
    } else {
        $sessionUUID = "theAwesomeUniqueSessionIdentifierFor" . appDisplayNameFromBundle($appBundle);
    }
    # FIXME: We should have the iOS application adopt the files descriptors for our standard output and error streams.
    my $sessionInfo = {
        applicationArguments => &$makeNSArrayFromArray($applicationArguments),
        applicationEnvironment => &$makeNSDictionaryFromHash($simulatorENVHashRef),
        applicationIdentifier => NSString->stringWithCString_(appIdentiferFromBundle($appBundle)),
        applicationPath => NSString->stringWithCString_($appBundle),
        deviceUDID => NSString->stringWithCString_($simulatedDevice->{UDID}),
        sessionUUID => NSString->stringWithCString_($sessionUUID),
    };

    openIOSSimulator();

    my $iPhoneSimulatorNotification = new IPhoneSimulatorNotification;
    $iPhoneSimulatorNotification->startObservingApplicationLaunchedNotification();
    $iPhoneSimulatorNotification->postStartSessionNotification($sessionInfo);
    while (!$iPhoneSimulatorNotification->hasReceivedApplicationLaunchedNotification()) {
        my $date = NSDate->alloc()->initWithTimeIntervalSinceNow_(0.1);
        NSRunLoop->currentRunLoop->runUntilDate_($date);
        $date->release();
    }
    $iPhoneSimulatorNotification->stopObservingApplicationLaunchedNotification();
    return $iPhoneSimulatorNotification->applicationLaunchedApplicationPID();
}

sub runIOSWebKitApp($)
{
    my ($appBundle) = @_;
    if (willUseIOSDeviceSDKWhenBuilding()) {
        die "Only running Safari in iOS Simulator is supported now.";
    }
    if (willUseIOSSimulatorSDKWhenBuilding()) {
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
    if (!shouldTargetWebProcess()) {
        print "Starting @{[basename($appPath)]} under $debugger with DYLD_FRAMEWORK_PATH set to point to built WebKit in $productDir.\n";
        exec { $debuggerPath } $debuggerPath, @architectureFlags, $argumentsSeparator, $appPath, argumentsForRunAndDebugMacWebKitApp() or die;
    } else {
        if (shouldUseXPCServiceForWebProcess()) {
            die "Targetting the Web Process is not compatible with using an XPC Service for the Web Process at this time.";
        }
        
        my $webProcessShimPath = File::Spec->catfile($productDir, "SecItemShim.dylib");
        my $webProcessPath = File::Spec->catdir($productDir, "WebProcess.app");
        my $webKit2ExecutablePath = File::Spec->catfile($productDir, "WebKit2.framework", "WebKit2");

        appendToEnvironmentVariableList("DYLD_INSERT_LIBRARIES", $webProcessShimPath);

        print "Starting WebProcess under $debugger with DYLD_FRAMEWORK_PATH set to point to built WebKit in $productDir.\n";
        exec { $debuggerPath } $debuggerPath, @architectureFlags, $argumentsSeparator, $webProcessPath, $webKit2ExecutablePath, "-type", "webprocess", "-client-executable", $appPath or die;
    }
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
        my $productDir = productDir();
        my $webKitLauncherPath = File::Spec->catfile(productDir(), "WinLauncher.exe");
        return system { $webKitLauncherPath } $webKitLauncherPath, @ARGV;
    }

    return 1; # Unsupported platform; can't run Safari on this platform.
}

sub runMiniBrowser
{
    if (isAppleMacWebKit()) {
        return runMacWebKitApp(File::Spec->catfile(productDir(), "MiniBrowser.app", "Contents", "MacOS", "MiniBrowser"));
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
    open UPDATE, "-|", "svn", "update", @svnOptions or die;
    my @conflictedChangeLogs;
    while (my $line = <UPDATE>) {
        print $line;
        $line =~ m/^C\s+(.+?)[\r\n]*$/;
        if ($1) {
          my $filename = normalizePath($1);
          push @conflictedChangeLogs, $filename if basename($filename) eq "ChangeLog";
        }
    }
    close UPDATE or die;

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
    if (isGitSVN()) {
        system("git", "svn", "rebase") == 0 or die;
    } else {
        # This will die if branch.$BRANCHNAME.merge isn't set, which is
        # almost certainly what we want.
        system("git", "pull") == 0 or die;
    }
}

1;
