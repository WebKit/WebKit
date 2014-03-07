# Copyright (C) 2005, 2006, 2007, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
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
use version;
use warnings;
use Config;
use Digest::MD5 qw(md5_hex);
use FindBin;
use File::Basename;
use File::Path qw(mkpath rmtree);
use File::Spec;
use File::stat;
use POSIX;
use VCSUtils;

BEGIN {
   use Exporter   ();
   our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
   $VERSION     = 1.00;
   @ISA         = qw(Exporter);
   @EXPORT      = qw(
       &XcodeOptionString
       &XcodeOptionStringNoConfig
       &XcodeOptions
       &baseProductDir
       &chdirWebKit
       &checkFrameworks
       &cmakeBasedPortArguments
       &cmakeBasedPortName
       &currentSVNRevision
       &debugSafari
       &nmPath
       &passedConfiguration
       &printHelpAndExitForRunAndDebugWebKitAppIfNeeded
       &productDir
       &runMacWebKitApp
       &safariPath
       &setConfiguration
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
my $baseProductDir;
my @baseProductDirOption;
my $configuration;
my $xcodeSDK;
my $configurationForVisualStudio;
my $configurationProductDir;
my $sourceDir;
my $currentSVNRevision;
my $debugger;
my $iPhoneSimulatorVersion;
my $nmPath;
my $osXVersion;
my $generateDsym;
my $isGtkAutotools;
my $isGtkCMake;
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

    if (isGtkAutotools()) {
        determineConfigurationProductDir();
        my $host_triple = `grep -E '^host = ' $configurationProductDir/GNUmakefile 2> /dev/null`;
        if ($host_triple =~ m/^host = ([^-]+)-/) {
            # We have a configured build tree; use it.
            $architecture = $1;
        }
    } elsif (isAppleMacWebKit()) {
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
    } elsif (isEfl() || isGtkCMake()) {
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
    } elsif (isDarwin() || isFreeBSD()) {
        chomp($numberOfCPUs = `sysctl -n hw.ncpu`);
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

    my @args = ();
    push(@args, '--debug') if ($configuration =~ "^Debug");
    push(@args, '--release') if ($configuration =~ "^Release");
    push(@args, '--32-bit') if ($architecture ne "x86_64" and !isWin64());
    push(@args, '--64-bit') if (isWin64());
    push(@args, '--gtkautotools') if isGtkAutotools();
    push(@args, '--gtk') if isGtkCMake();
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
    } elsif (checkForArgumentAndRemoveFromARGV("--device")) {
        $xcodeSDK = 'iphoneos.internal';
    } elsif (checkForArgumentAndRemoveFromARGV("--sim") ||
        checkForArgumentAndRemoveFromARGV("--simulator")) {
        $xcodeSDK = 'iphonesimulator';
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
    $productDir .= "/bin" if (isEfl() || isGtkCMake());
    $productDir .= "/Programs" if isGtkAutotools();

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

    if (isAppleMacWebKit()) {
        my $safariPath = "$safariBundle/Contents/MacOS/Safari";
        return $safariPath if skipSafariExecutableEntitlementChecks();

        my $safariForWebKitDevelopmentPath = "$safariBundle/Contents/MacOS/SafariForWebKitDevelopment";
        return $safariForWebKitDevelopmentPath if -f $safariForWebKitDevelopmentPath && executableHasEntitlements($safariPath);

        return $safariPath;
    }
    return $safariBundle if isAppleWinWebKit();
}

sub installedSafariPath
{
    my $safariBundle;

    if (isAppleMacWebKit()) {
        $safariBundle = "/Applications/Safari.app";
    } elsif (isAppleWinWebKit()) {
        $safariBundle = readRegistryString("/HKLM/SOFTWARE/Apple Computer, Inc./Safari/InstallDir");
        $safariBundle =~ s/[\r\n]+$//;
        $safariBundle = `cygpath -u '$safariBundle'` if isCygwin();
        $safariBundle =~ s/[\r\n]+$//;
        $safariBundle .= "Safari.exe";
    }

    return safariPathFromSafariBundle($safariBundle);
}

# Locate Safari.
sub safariPath
{
    # Use WEBKIT_SAFARI environment variable if present.
    my $safariBundle = $ENV{WEBKIT_SAFARI};
    if (!$safariBundle) {
        determineConfigurationProductDir();
        # Use Safari.app in product directory if present (good for Safari development team).
        if (isAppleMacWebKit() && -d "$configurationProductDir/Safari.app") {
            $safariBundle = "$configurationProductDir/Safari.app";
        } elsif (isAppleWinWebKit()) {
            my $path = "$configurationProductDir/Safari.exe";
            my $debugPath = "$configurationProductDir/Safari_debug.exe";

            if (configuration() eq "Debug_All" && -x $debugPath) {
                $safariBundle = $debugPath;
            } elsif (-x $path) {
                $safariBundle = $path;
            }
        }
        if (!$safariBundle) {
            return installedSafariPath();
        }
    }
    my $safariPath = safariPathFromSafariBundle($safariBundle);
    die "Can't find executable at $safariPath.\n" if isAppleMacWebKit() && !-x $safariPath;
    return $safariPath;
}

sub builtDylibPathForName
{
    my $libraryName = shift;
    determineConfigurationProductDir();

    if (isGtk()) {
        # WebKitGTK+ for GTK2, WebKitGTK+ for GTK3, and WebKit2 respectively.
        my @libraries = ("libwebkitgtk-1.0", "libwebkitgtk-3.0", "libwebkit2gtk-3.0");
        my $extension = isDarwin() ? ".dylib" : ".so";

        my $builtLibraryPath = "$configurationProductDir/.libs/";
        if (isGtkCMake()) {
            $builtLibraryPath = "$configurationProductDir/lib/";
        }

        foreach $libraryName (@libraries) {
            my $libraryPath = "$builtLibraryPath" . $libraryName . $extension;
            return $libraryPath if -e $libraryPath;
        }
        return "NotFound";
    }
    if (isEfl()) {
        if (isWK2()) {
            return "$configurationProductDir/lib/libewebkit2.so";
        }
        return "$configurationProductDir/lib/libewebkit.so";
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

sub determineIsGtkCMake()
{
    return if defined($isGtkCMake);
    $isGtkCMake = checkForArgumentAndRemoveFromARGV("--gtk");
}

sub isGtkCMake()
{
    determineIsGtkCMake();
    return $isGtkCMake;
}

sub isGtkAutotools()
{
    determineIsGtkAutotools();
    return $isGtkAutotools;
}

sub isGtk()
{
    return isGtkCMake() || isGtkAutotools();
}

sub determineIsGtkAutotools()
{
    return if defined($isGtkAutotools);
    $isGtkAutotools = checkForArgumentAndRemoveFromARGV("--gtkautotools");
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

sub isFreeBSD()
{
    return ($^O eq "freebsd") || 0;
}

sub isARM()
{
    return $Config{archname} =~ /^arm[v\-]/;
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

sub isPerianInstalled()
{
    if (!isAppleWebKit()) {
        return 0;
    }

    if (-d "/Library/QuickTime/Perian.component") {
        return 1;
    }

    if (-d "$ENV{HOME}/Library/QuickTime/Perian.component") {
        return 1;
    }

    return 0;
}

sub determineIPhoneSimulatorVersion()
{
    return if $iPhoneSimulatorVersion;

    if (!isIOSWebKit()) {
        $iPhoneSimulatorVersion = -1;
        return;
    }

    my $version = `/usr/local/bin/psw_vers -productVersion`;
    my @splitVersion = split(/\./, $version);
    @splitVersion >= 2 or die "Invalid version $version";
    $iPhoneSimulatorVersion = {
            "major" => $splitVersion[0],
            "minor" => $splitVersion[1],
            "subminor" => defined($splitVersion[2] ? $splitVersion[2] : 0),
    };
}

sub iPhoneSimulatorVersion()
{
    determineIPhoneSimulatorVersion();
    return $iPhoneSimulatorVersion;
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

sub isSnowLeopard()
{
    return isDarwin() && osXVersion()->{"minor"} == 6;
}

sub isLion()
{
    return isDarwin() && osXVersion()->{"minor"} == 7;
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

sub setUpGuardMallocIfNeeded
{
    if (!isDarwin()) {
        return;
    }

    if (!defined($shouldUseGuardMalloc)) {
        $shouldUseGuardMalloc = checkForArgumentAndRemoveFromARGV("--guard-malloc");
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
    if (isGtk()) {
        return "GtkLauncher";
    } elsif (isAppleMacWebKit()) {
        return "Safari";
    } elsif (isAppleWinWebKit()) {
        return "WinLauncher";
    } elsif (isEfl()) {
        return "EWebLauncher/MiniBrowser";
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

    # Python 2.7.3 as shipped by Cygwin prevents the build from completing due to some issue with handling of
    # environment variables. Avoid until this is corrected.
    my $pythonVer = `python --version 2>&1`;
    die "You must have Python installed to build WebKit.\n" if ($?);
    die "Python 2.7.3 is not compatible with the WebKit build. Please downgrade to Python 2.6.8.\n" if ($pythonVer =~ /2\.7\.3/);

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

sub copyInspectorFrontendFiles
{
    my $productDir = productDir();
    my $sourceInspectorPath = sourceDir() . "/Source/WebCore/inspector/front-end/";
    my $inspectorResourcesDirPath = $ENV{"WEBKITINSPECTORRESOURCESDIR"};

    if (!defined($inspectorResourcesDirPath)) {
        $inspectorResourcesDirPath = "";
    }

    if (isAppleMacWebKit()) {
        if (isIOSWebKit()) {
            $inspectorResourcesDirPath = $productDir . "/WebCore.framework/inspector";
        } else {
            $inspectorResourcesDirPath = $productDir . "/WebCore.framework/Resources/inspector";
        }
    } elsif (isAppleWinWebKit() || isWinCairo()) {
        $inspectorResourcesDirPath = $productDir . "/WebKit.resources/inspector";
    } elsif (isGtk()) {
        my $prefix = $ENV{"WebKitInstallationPrefix"};
        $inspectorResourcesDirPath = (defined($prefix) ? $prefix : "/usr/share") . "/webkit-1.0/webinspector";
    } elsif (isEfl()) {
        my $prefix = $ENV{"WebKitInstallationPrefix"};
        $inspectorResourcesDirPath = (defined($prefix) ? $prefix : "/usr/share") . "/ewebkit/webinspector";
    }

    if (! -d $inspectorResourcesDirPath) {
        print "*************************************************************\n";
        print "Cannot find '$inspectorResourcesDirPath'.\n" if (defined($inspectorResourcesDirPath));
        print "Make sure that you have built WebKit first.\n" if (! -d $productDir || defined($inspectorResourcesDirPath));
        print "Optionally, set the environment variable 'WebKitInspectorResourcesDir'\n";
        print "to point to the directory that contains the WebKit Inspector front-end\n";
        print "files for the built WebCore framework.\n";
        print "*************************************************************\n";
        die;
    }

    if (isAppleMacWebKit()) {
        my $sourceLocalizedStrings = sourceDir() . "/Source/WebCore/English.lproj/localizedStrings.js";
        my $destinationLocalizedStrings;
        if (isIOSWebKit()) {
            $destinationLocalizedStrings = $productDir . "/WebCore.framework/English.lproj/localizedStrings.js";
        } else {
            $destinationLocalizedStrings = $productDir . "/WebCore.framework/Resources/English.lproj/localizedStrings.js";
        }
        system "ditto", $sourceLocalizedStrings, $destinationLocalizedStrings;
    }

    my $exitStatus = system "rsync", "-aut", "--exclude=/.DS_Store", "--exclude=*.re2js", "--exclude=.svn/", $sourceInspectorPath, $inspectorResourcesDirPath;
    return $exitStatus if $exitStatus;

    if (isIOSWebKit()) {
        chdir($productDir . "/WebCore.framework");
        return system "zip", "--quiet", "--exclude=*.qrc", "-r", "inspector-remote.zip", "inspector";
    }

    return 0; # Success; did copy files.
}

sub buildXCodeProject($$@)
{
    my ($project, $clean, @extraOptions) = @_;

    if ($clean) {
        push(@extraOptions, "-alltargets");
        push(@extraOptions, "clean");
    }

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

sub autotoolsFlag($$)
{
    my ($flag, $feature) = @_;
    my $prefix = $flag ? "--enable" : "--disable";

    return $prefix . '-' . $feature;
}

sub runAutogenForAutotoolsProjectIfNecessary($@)
{
    my ($dir, $prefix, $sourceDir, $project, $joinedOverridableFeatures, @buildArgs) = @_;

    # Always enable introspection when building WebKitGTK+.
    unshift(@buildArgs, "--enable-introspection");

    # Also, always enable developer mode for developer/test builds.
    unshift(@buildArgs, "--enable-developer-mode");

    # Optimize for running WebKit inside the build tree
    unshift(@buildArgs, "--disable-fast-install");

    my $joinedBuildArgs = join(" ", @buildArgs);

    if (-e "GNUmakefile") {
        # Just assume that build-jsc will never be used to reconfigure JSC. Later
        # we can go back and make this more complicated if the demand is there.
        if ($project ne "WebKit") {
            return;
        }

        # Run autogen.sh again if either the features overrided by build-webkit or build arguments have changed.
        if (!isCachedArgumentfileOutOfDate("WebKitFeatureOverrides.txt", $joinedOverridableFeatures)
            && !isCachedArgumentfileOutOfDate("previous-autogen-arguments.txt", $joinedBuildArgs)) {
            return;
        }
    }

    print "Calling autogen.sh in " . $dir . "\n\n";
    print "Installation prefix directory: $prefix\n" if(defined($prefix));

    # Only for WebKit, write the autogen.sh arguments to a file so that we can detect
    # when they change and automatically re-run it.
    if ($project eq 'WebKit') {
        open(OVERRIDABLE_FEATURES, ">WebKitFeatureOverrides.txt");
        print OVERRIDABLE_FEATURES $joinedOverridableFeatures;
        close(OVERRIDABLE_FEATURES);

        open(AUTOTOOLS_ARGUMENTS, ">previous-autogen-arguments.txt");
        print AUTOTOOLS_ARGUMENTS $joinedBuildArgs;
        close(AUTOTOOLS_ARGUMENTS);
    }

    # Make the path relative since it will appear in all -I compiler flags.
    # Long argument lists cause bizarre slowdowns in libtool.
    my $relSourceDir = File::Spec->abs2rel($sourceDir) || ".";

    # Compiler options to keep floating point values consistent
    # between 32-bit and 64-bit architectures. The options are also
    # used on Chromium build.
    determineArchitecture();
    if ($architecture ne "x86_64" && !isARM() && !isCrossCompilation()) {
        $ENV{'CXXFLAGS'} = "-march=pentium4 -msse2 -mfpmath=sse " . ($ENV{'CXXFLAGS'} || "");
    }

    # Prefix the command with jhbuild run.
    unshift(@buildArgs, "$relSourceDir/autogen.sh");
    unshift(@buildArgs, jhbuildWrapperPrefixIfNeeded());
    if (system(@buildArgs) ne 0) {
        die "Calling autogen.sh failed!\n";
    }
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

sub buildAutotoolsProject($@)
{
    my ($project, $clean, $prefix, $makeArgs, $noWebKit1, $noWebKit2, @features) = @_;

    my $make = 'make';
    my $dir = productDir();
    my $config = passedConfiguration() || configuration();

    # Use rm to clean the build directory since distclean may miss files
    if ($clean && -d $dir) {
        system "rm", "-rf", "$dir";
    }

    if (! -d $dir) {
        File::Path::mkpath($dir) or die "Failed to create build directory " . $dir
    }
    chdir $dir or die "Failed to cd into " . $dir . "\n";

    if ($clean) {
        return 0;
    }

    my @buildArgs = @ARGV;
    if ($noWebKit1) {
        unshift(@buildArgs, "--disable-webkit1");
    }
    if ($noWebKit2) {
        unshift(@buildArgs, "--disable-webkit2");
    }

    # Configurable features listed here should be kept in sync with the
    # features for which there exists a configuration option in configure.ac.
    my %configurableFeatures = (
        "battery-status" => 1,
        "gamepad" => 1,
        "geolocation" => 1,
        "svg" => 1,
        "svg-fonts" => 1,
        "video" => 1,
        "webgl" => 1,
        "web-audio" => 1,
    );

    # These features are ones which build-webkit cannot control, typically because
    # they can only be active when we have the proper dependencies.
    my %unsetFeatures = (
        "accelerated-2d-canvas" => 1,
    );

    my @overridableFeatures = ();
    foreach (@features) {
        if ($configurableFeatures{$_->{option}}) {
            push @buildArgs, autotoolsFlag(${$_->{value}}, $_->{option});;
        } elsif (!$unsetFeatures{$_->{option}}) {
            push @overridableFeatures, $_->{define} . "=" . (${$_->{value}} ? "1" : "0");
        }
    }

    $makeArgs = $makeArgs || "";
    $makeArgs = $makeArgs . " " . $ENV{"WebKitMakeArguments"} if $ENV{"WebKitMakeArguments"};

    # Automatically determine the number of CPUs for make only
    # if make arguments haven't already been specified.
    if ($makeArgs eq "") {
        $makeArgs = "-j" . numberOfCPUs();
    }

    # WebKit is the default target, so we don't need to specify anything.
    if ($project eq "JavaScriptCore") {
        $makeArgs .= " jsc";
    } elsif ($project eq "WTF") {
        $makeArgs .= " libWTF.la";
    }

    $prefix = $ENV{"WebKitInstallationPrefix"} if !defined($prefix);
    push @buildArgs, "--prefix=" . $prefix if defined($prefix);

    # Check if configuration is Debug.
    my $debug = $config =~ m/debug/i;
    if ($debug) {
        push @buildArgs, "--enable-debug";
    } else {
        push @buildArgs, "--disable-debug";
    }

    if (checkForArgumentAndRemoveFromArrayRef("--update-gtk", \@buildArgs)) {
        # Force autogen to run, to catch the possibly updated libraries.
        system("rm -f previous-autogen-arguments.txt");

        system("perl", "$sourceDir/Tools/Scripts/update-webkitgtk-libs") == 0 or die $!;
    }

    # If GNUmakefile exists, don't run autogen.sh unless its arguments
    # have changed. The makefile should be smart enough to track autotools
    # dependencies and re-run autogen.sh when build files change.
    my $joinedOverridableFeatures = join(" ", @overridableFeatures);
    runAutogenForAutotoolsProjectIfNecessary($dir, $prefix, $sourceDir, $project, $joinedOverridableFeatures, @buildArgs);

    my $runWithJhbuild = join(" ", jhbuildWrapperPrefixIfNeeded());
    if (system("$runWithJhbuild $make $makeArgs") ne 0) {
        die "\nFailed to build WebKit using '$make'!\n";
    }

    chdir ".." or die;

    if (!checkForArgumentAndRemoveFromARGV("--disable-gtk-doc")) {
        if ($project eq 'WebKit' && !isCrossCompilation() && !($noWebKit1 && $noWebKit2)) {
            my @docGenerationOptions = ("$sourceDir/Tools/gtk/generate-gtkdoc", "--skip-html");
            unshift(@docGenerationOptions, jhbuildWrapperPrefixIfNeeded());

            if (system(@docGenerationOptions)) {
                die "\n gtkdoc did not build without warnings\n";
            }
        }
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
    my $optionsCache = File::Spec->catdir(baseProductDir(), configuration(), "build-webkit-options.txt");
    my $joinedBuildArgs = join(" ", @buildArgs);
    if (isCachedArgumentfileOutOfDate($optionsCache, $joinedBuildArgs)) {
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
    system('ninja --version > /dev/null');
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
    push @args, "-DSHARED_CORE=ON" if isEfl() && $ENV{"ENABLE_DRT"};
    if ($config =~ /release/i) {
        push @args, "-DCMAKE_BUILD_TYPE=Release";
    } elsif ($config =~ /debug/i) {
        push @args, "-DCMAKE_BUILD_TYPE=Debug";
    }

    if ($willUseNinja) {
        push @args, "-G";
        push @args, "Ninja";
    }

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
    my @args = ("--build", $buildPath, "--config", $config);
    push @args, ("--", $makeArgs) if $makeArgs;

    # We call system("cmake @args") instead of system("cmake", @args) so that @args is
    # parsed for shell metacharacters. In particular, $makeArgs may contain such metacharacters.
    my $wrapper = join(" ", jhbuildWrapperPrefixIfNeeded()) . " ";
    return system($wrapper . "cmake @args");
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
    return "GTK" if isGtkCMake();
    return "";
}

sub isCMakeBuild()
{
    return isEfl() || isWinCE() || isGtkCMake();
}

sub promptUser
{
    my ($prompt, $default) = @_;
    my $defaultValue = $default ? "[$default]" : "";
    print "$prompt $defaultValue: ";
    chomp(my $input = <STDIN>);
    return $input ? $input : $default;
}

sub buildGtkProject
{
    my ($project, $clean, $prefix, $makeArgs, $noWebKit1, $noWebKit2, @features) = @_;

    if ($project ne "WebKit" and $project ne "JavaScriptCore" and $project ne "WTF") {
        die "Unsupported project: $project. Supported projects: WebKit, JavaScriptCore, WTF\n";
    }

    return buildAutotoolsProject($project, $clean, $prefix, $makeArgs, $noWebKit1, $noWebKit2, @features);
}

sub appleApplicationSupportPath
{
    if (isWin64()) {
        # FIXME (125180): Remove the following once official 64-bit Windows support is available.
        return $ENV{"WEBKIT_64_SUPPORT"}, "\n" if isWin64();
    }

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
        $env->{PATH} = join(':', productDir(), dirname(installedSafariPath()), appleApplicationSupportPath(), $env->{PATH} || "");
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
  --guard-malloc                    Enable Guard Malloc (OS X only)
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

sub runMacWebKitApp($;$)
{
    my ($appPath, $useOpenCommand) = @_;
    my $productDir = productDir();
    print "Starting @{[basename($appPath)]} with DYLD_FRAMEWORK_PATH set to point to built WebKit in $productDir.\n";
    $ENV{DYLD_FRAMEWORK_PATH} = $productDir;
    $ENV{WEBKIT_UNSET_DYLD_FRAMEWORK_PATH} = "YES";

    setUpGuardMallocIfNeeded();

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
    $ENV{DYLD_FRAMEWORK_PATH} = $productDir;
    $ENV{WEBKIT_UNSET_DYLD_FRAMEWORK_PATH} = "YES";

    setUpGuardMallocIfNeeded();

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
    } elsif (isGtk()) {
        my $productDir = productDir();
        my $injectedBundlePath = "$productDir/Libraries/.libs/libTestRunnerInjectedBundle";
        print "Starting WebKitTestRunner with TEST_RUNNER_INJECTED_BUNDLE_FILENAME set to point to $injectedBundlePath.\n";
        $ENV{TEST_RUNNER_INJECTED_BUNDLE_FILENAME} = $injectedBundlePath;
        my @args = ("$productDir/Programs/WebKitTestRunner", @ARGV);
        return system {$args[0] } @args;
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

1;
