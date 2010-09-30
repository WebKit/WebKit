# Copyright (C) 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
# Copyright (C) 2009 Google Inc. All rights reserved.
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
use Config;
use FindBin;
use File::Basename;
use File::Path;
use File::Spec;
use POSIX;
use VCSUtils;

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

my $architecture;
my $numberOfCPUs;
my $baseProductDir;
my @baseProductDirOption;
my $configuration;
my $configurationForVisualStudio;
my $configurationProductDir;
my $sourceDir;
my $currentSVNRevision;
my $osXVersion;
my $isQt;
my $isSymbian;
my %qtFeatureDefaults;
my $isGtk;
my $isWx;
my $isEfl;
my @wxArgs;
my $isChromium;
my $isInspectorFrontend;

# Variables for Win32 support
my $vcBuildPath;
my $windowsTmpPath;
my $windowsSourceDir;
my $winVersion;
my $willUseVCExpressWhenBuilding = 0;

# Defined in VCSUtils.
sub exitStatus($);

sub determineSourceDir
{
    return if $sourceDir;
    $sourceDir = $FindBin::Bin;
    $sourceDir =~ s|/+$||; # Remove trailing '/' as we would die later

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

sub determineBaseProductDir
{
    return if defined $baseProductDir;
    determineSourceDir();

    $baseProductDir = $ENV{"WEBKITOUTPUTDIR"};

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

        open PRODUCT, "defaults read com.apple.Xcode PBXApplicationwideBuildSettings 2> " . File::Spec->devnull() . " |" or die;
        $baseProductDir = join '', <PRODUCT>;
        close PRODUCT;

        $baseProductDir = $1 if $baseProductDir =~ /SYMROOT\s*=\s*\"(.*?)\";/s;
        undef $baseProductDir unless $baseProductDir =~ /^\//;

        if (!defined($baseProductDir)) {
            open PRODUCT, "defaults read com.apple.Xcode PBXProductDirectory 2> " . File::Spec->devnull() . " |" or die;
            $baseProductDir = <PRODUCT>;
            close PRODUCT;
            if ($baseProductDir) {
                chomp $baseProductDir;
                undef $baseProductDir unless $baseProductDir =~ /^\//;
            }
        }
    } elsif (isSymbian()) {
        # Shadow builds are not supported on Symbian
        $baseProductDir = $sourceDir;
    }

    if (!defined($baseProductDir)) { # Port-spesific checks failed, use default
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
    }

    if (isCygwin()) {
        my $dosBuildPath = `cygpath --windows \"$baseProductDir\"`;
        chomp $dosBuildPath;
        $ENV{"WEBKITOUTPUTDIR"} = $dosBuildPath;
    }

    if (isAppleWinWebKit()) {
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
}

sub determineArchitecture
{
    return if defined $architecture;
    # make sure $architecture is defined for non-apple-mac builds
    $architecture = "";
    return unless isAppleMacWebKit();

    determineBaseProductDir();
    if (open ARCHITECTURE, "$baseProductDir/Architecture") {
        $architecture = <ARCHITECTURE>;
        close ARCHITECTURE;
    }
    if ($architecture) {
        chomp $architecture;
    } else {
        if (isTiger() or isLeopard()) {
            $architecture = `arch`;
        } else {
            my $supports64Bit = `sysctl -n hw.optional.x86_64`;
            chomp $supports64Bit;
            $architecture = $supports64Bit ? 'x86_64' : `arch`;
        }
        chomp $architecture;
    }
}

sub determineNumberOfCPUs
{
    return if defined $numberOfCPUs;
    if (isLinux()) {
        # First try the nproc utility, if it exists. If we get no
        # results fall back to just interpretting /proc directly.
        chomp($numberOfCPUs = `nproc 2> /dev/null`);
        if ($numberOfCPUs eq "") {
            $numberOfCPUs = (grep /processor/, `cat /proc/cpuinfo`);
        }
    } elsif (isWindows() || isCygwin()) {
        if (defined($ENV{NUMBER_OF_PROCESSORS})) {
            $numberOfCPUs = $ENV{NUMBER_OF_PROCESSORS};
        } else {
            # Assumes cygwin
            $numberOfCPUs = `ls /proc/registry/HKEY_LOCAL_MACHINE/HARDWARE/DESCRIPTION/System/CentralProcessor | wc -w`;
        }
    } elsif (isDarwin()) {
        $numberOfCPUs = `sysctl -n hw.ncpu`;
    }
}

sub jscPath($)
{
    my ($productDir) = @_;
    my $jscName = "jsc";
    $jscName .= "_debug"  if (isCygwin() && ($configuration eq "Debug"));
    $jscName .= ".exe" if (isWindows() || isCygwin());
    return "$productDir/$jscName" if -e "$productDir/$jscName";
    return "$productDir/JavaScriptCore.framework/Resources/$jscName";
}

sub argumentsForConfiguration()
{
    determineConfiguration();
    determineArchitecture();

    my @args = ();
    push(@args, '--debug') if $configuration eq "Debug";
    push(@args, '--release') if $configuration eq "Release";
    push(@args, '--32-bit') if $architecture ne "x86_64";
    push(@args, '--qt') if isQt();
    push(@args, '--symbian') if isSymbian();
    push(@args, '--gtk') if isGtk();
    push(@args, '--efl') if isEfl();
    push(@args, '--wx') if isWx();
    push(@args, '--chromium') if isChromium();
    push(@args, '--inspector-frontend') if isInspectorFrontend();
    return @args;
}

sub determineConfigurationForVisualStudio
{
    return if defined $configurationForVisualStudio;
    determineConfiguration();
    $configurationForVisualStudio = $configuration;
    return unless $configuration eq "Debug";
    setupCygwinEnv();
    chomp(my $dir = `cygpath -ua '$ENV{WEBKITLIBRARIESDIR}'`);
    $configurationForVisualStudio = "Debug_Internal" if -f "$dir/bin/CoreFoundation_debug.dll";
}

sub determineConfigurationProductDir
{
    return if defined $configurationProductDir;
    determineBaseProductDir();
    determineConfiguration();
    if (isAppleWinWebKit() && !isWx()) {
        $configurationProductDir = "$baseProductDir/bin";
    } else {
        # [Gtk][Efl] We don't have Release/Debug configurations in straight
        # autotool builds (non build-webkit). In this case and if
        # WEBKITOUTPUTDIR exist, use that as our configuration dir. This will
        # allows us to run run-webkit-tests without using build-webkit.
        if ($ENV{"WEBKITOUTPUTDIR"} && (isGtk() || isEfl())) {
            $configurationProductDir = "$baseProductDir";
        } else {
            $configurationProductDir = "$baseProductDir/$configuration";
        }
    }
}

sub setConfigurationProductDir($)
{
    ($configurationProductDir) = @_;
}

sub determineCurrentSVNRevision
{
    return if defined $currentSVNRevision;
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
    $productDir .= "/JavaScriptCore" if isQt();
    $productDir .= "/$configuration" if (isQt() && isWindows());
    $productDir .= "/Programs" if (isGtk() || isEfl());

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
    determineCurrentSVNRevision();
    return $currentSVNRevision;
}

sub XcodeOptions
{
    determineBaseProductDir();
    determineConfiguration();
    determineArchitecture();
    return (@baseProductDirOption, "-configuration", $configuration, "ARCHS=$architecture");
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
    push @coverageSupportOptions, "EXTRA_LINK= \$(EXTRA_LINK) -ftest-coverage -fprofile-arcs";
    push @coverageSupportOptions, "OTHER_CFLAGS= \$(OTHER_CFLAGS) -DCOVERAGE -MD";
    push @coverageSupportOptions, "OTHER_LDFLAGS=\$(OTHER_LDFLAGS) -ftest-coverage -fprofile-arcs -lgcov";
    return @coverageSupportOptions;
}

my $passedConfiguration;
my $searchedForPassedConfiguration;
sub determinePassedConfiguration
{
    return if $searchedForPassedConfiguration;
    $searchedForPassedConfiguration = 1;

    my $isWinCairo = checkForArgumentAndRemoveFromARGV("--wincairo");

    for my $i (0 .. $#ARGV) {
        my $opt = $ARGV[$i];
        if ($opt =~ /^--debug$/i || $opt =~ /^--devel/i) {
            splice(@ARGV, $i, 1);
            $passedConfiguration = "Debug";
            $passedConfiguration .= "_Cairo" if ($isWinCairo && isCygwin());
            return;
        }
        if ($opt =~ /^--release$/i || $opt =~ /^--deploy/i) {
            splice(@ARGV, $i, 1);
            $passedConfiguration = "Release";
            $passedConfiguration .= "_Cairo" if ($isWinCairo && isCygwin());
            return;
        }
        if ($opt =~ /^--profil(e|ing)$/i) {
            splice(@ARGV, $i, 1);
            $passedConfiguration = "Profiling";
            $passedConfiguration .= "_Cairo" if ($isWinCairo && isCygwin());
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

    for my $i (0 .. $#ARGV) {
        my $opt = $ARGV[$i];
        if ($opt =~ /^--32-bit$/i) {
            splice(@ARGV, $i, 1);
            if (isAppleMacWebKit()) {
                $passedArchitecture = `arch`;
                chomp $passedArchitecture;
            }
            return;
        }
    }
    $passedArchitecture = undef;
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


sub safariPathFromSafariBundle
{
    my ($safariBundle) = @_;

    return "$safariBundle/Contents/MacOS/Safari" if isAppleMacWebKit();
    return $safariBundle if isAppleWinWebKit();
}

sub installedSafariPath
{
    my $safariBundle;

    if (isAppleMacWebKit()) {
        $safariBundle = "/Applications/Safari.app";
    } elsif (isAppleWinWebKit()) {
        $safariBundle = `"$configurationProductDir/FindSafari.exe"`;
        $safariBundle =~ s/[\r\n]+$//;
        $safariBundle = `cygpath -u '$safariBundle'`;
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

            if (configurationForVisualStudio() =~ /Debug/ && -x $debugPath) {
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
    if (isChromium()) {
        return "$configurationProductDir/$libraryName";
    }
    if (isQt()) {
        $libraryName = "QtWebKit";
        if (isDarwin() and -d "$configurationProductDir/lib/$libraryName.framework") {
            return "$configurationProductDir/lib/$libraryName.framework/$libraryName";
        } elsif (isWindows()) {
            if (configuration() eq "Debug") {
                # On Windows, there is a "d" suffix to the library name. See <http://trac.webkit.org/changeset/53924/>.
                $libraryName .= "d";
            }

            my $mkspec = `qmake -query QMAKE_MKSPECS`;
            $mkspec =~ s/[\n|\r]$//g;
            my $qtMajorVersion = retrieveQMakespecVar("$mkspec/qconfig.pri", "QT_MAJOR_VERSION");
            if (not $qtMajorVersion) {
                $qtMajorVersion = "";
            }
            return "$configurationProductDir/lib/$libraryName$qtMajorVersion.dll";
        } else {
            return "$configurationProductDir/lib/lib$libraryName.so";
        }
    }
    if (isWx()) {
        return "$configurationProductDir/libwxwebkit.dylib";
    }
    if (isGtk()) {
        my $libraryDir = "$configurationProductDir/$libraryName/../.libs/";
        if (-e $libraryDir . "libwebkitgtk-3.0.so") {
            return $libraryDir . "libwebkitgtk-3.0.so";
        }
        return $libraryDir . "libwebkitgtk-1.0.so";
    }
    if (isEfl()) {
        return "$configurationProductDir/$libraryName/../.libs/libewebkit.so";
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
    return if isCygwin();
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

sub isQt()
{
    determineIsQt();
    return $isQt;
}

sub isSymbian()
{
    determineIsSymbian();
    return $isSymbian;
}

sub qtFeatureDefaults()
{
    determineQtFeatureDefaults();
    return %qtFeatureDefaults;
}

sub commandExists($)
{
    my $command = shift;
    my $devnull = File::Spec->devnull();
    return `$command --version 2> $devnull`;
}

sub determineQtFeatureDefaults()
{
    return if %qtFeatureDefaults;
    die "ERROR: qmake missing but required to build WebKit.\n" if not commandExists("qmake");
    my $originalCwd = getcwd();
    chdir File::Spec->catfile(sourceDir(), "WebCore");
    my $defaults = `qmake CONFIG+=compute_defaults 2>&1`;
    chdir $originalCwd;

    while ($defaults =~ m/(\S+?)=(\S+?)/gi) {
        $qtFeatureDefaults{$1}=$2;
    }
}

sub checkForArgumentAndRemoveFromARGV
{
    my $argToCheck = shift;
    foreach my $opt (@ARGV) {
        if ($opt =~ /^$argToCheck$/i ) {
            @ARGV = grep(!/^$argToCheck$/i, @ARGV);
            return 1;
        }
    }
    return 0;
}

sub determineIsQt()
{
    return if defined($isQt);

    # Allow override in case QTDIR is not set.
    if (checkForArgumentAndRemoveFromARGV("--qt")) {
        $isQt = 1;
        return;
    }

    # The presence of QTDIR only means Qt if --gtk or --wx or --efl are not on the command-line
    if (isGtk() || isWx() || isEfl()) {
        $isQt = 0;
        return;
    }
    
    $isQt = defined($ENV{'QTDIR'});
}

sub determineIsSymbian()
{
    return if defined($isSymbian);

    if (checkForArgumentAndRemoveFromARGV("--symbian")) {
        $isSymbian = 1;
        return;
    }
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

sub isGtk()
{
    determineIsGtk();
    return $isGtk;
}

sub determineIsGtk()
{
    return if defined($isGtk);
    $isGtk = checkForArgumentAndRemoveFromARGV("--gtk");
}

sub isWx()
{
    determineIsWx();
    return $isWx;
}

sub determineIsWx()
{
    return if defined($isWx);
    $isWx = checkForArgumentAndRemoveFromARGV("--wx");
}

sub getWxArgs()
{
    if (!@wxArgs) {
        @wxArgs = ("");
        my $rawWxArgs = "";
        foreach my $opt (@ARGV) {
            if ($opt =~ /^--wx-args/i ) {
                @ARGV = grep(!/^--wx-args/i, @ARGV);
                $rawWxArgs = $opt;
                $rawWxArgs =~ s/--wx-args=//i;
            }
        }
        @wxArgs = split(/,/, $rawWxArgs);
    }
    return @wxArgs;
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

sub isChromium()
{
    determineIsChromium();
    return $isChromium;
}

sub determineIsChromium()
{
    return if defined($isChromium);
    $isChromium = checkForArgumentAndRemoveFromARGV("--chromium");
}

sub isCygwin()
{
    return ($^O eq "cygwin") || 0;
}

sub determineWinVersion()
{
    return if $winVersion;

    if (!isCygwin()) {
        $winVersion = -1;
        return;
    }

    my $versionString = `uname -s`;
    $versionString =~ /(\d\.\d)/;
    $winVersion = $1;
}

sub winVersion()
{
    determineWinVersion();
    return $winVersion;
}

sub isWindows7()
{
    return winVersion() eq "6.1";
}

sub isWindowsVista()
{
    return winVersion() eq "6.0";
}

sub isWindowsXP()
{
    return winVersion() eq "5.1";
}

sub isDarwin()
{
    return ($^O eq "darwin") || 0;
}

sub isWindows()
{
    return ($^O eq "MSWin32") || 0;
}

sub isMsys()
{
    return ($^O eq "msys") || 0;
}

sub isLinux()
{
    return ($^O eq "linux") || 0;
}

sub isAppleWebKit()
{
    return !(isQt() or isGtk() or isWx() or isChromium() or isEfl());
}

sub isAppleMacWebKit()
{
    return isAppleWebKit() && isDarwin();
}

sub isAppleWinWebKit()
{
    return isAppleWebKit() && isCygwin();
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

sub isTiger()
{
    return isDarwin() && osXVersion()->{"minor"} == 4;
}

sub isLeopard()
{
    return isDarwin() && osXVersion()->{"minor"} == 5;
}

sub isSnowLeopard()
{
    return isDarwin() && osXVersion()->{"minor"} == 6;
}

sub isWindowsNT()
{
    return $ENV{'OS'} eq 'Windows_NT';
}

sub relativeScriptsDir()
{
    my $scriptDir = File::Spec->catpath("", File::Spec->abs2rel(dirname($0), getcwd()), "");
    if ($scriptDir eq "") {
        $scriptDir = ".";
    }
    return $scriptDir;
}

sub launcherPath()
{
    my $relativeScriptsPath = relativeScriptsDir();
    if (isGtk() || isQt() || isWx() || isEfl()) {
        return "$relativeScriptsPath/run-launcher";
    } elsif (isAppleWebKit()) {
        return "$relativeScriptsPath/run-safari";
    }
}

sub launcherName()
{
    if (isGtk()) {
        return "GtkLauncher";
    } elsif (isQt()) {
        return "QtTestBrowser";
    } elsif (isWx()) {
        return "wxBrowser";
    } elsif (isAppleWebKit()) {
        return "Safari";
    } elsif (isEfl()) {
        return "EWebLauncher";
    }
}

sub checkRequiredSystemConfig
{
    if (isDarwin()) {
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
    } elsif (isGtk() or isQt() or isWx() or isEfl()) {
        my @cmds = qw(flex bison gperf);
        my @missing = ();
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
    my $sourceDir = sourceDir();
    chomp($windowsSourceDir = `cygpath -w '$sourceDir'`);
}

sub windowsSourceDir()
{
    determineWindowsSourceDir();
    return $windowsSourceDir;
}

sub windowsLibrariesDir()
{
    return windowsSourceDir() . "\\WebKitLibraries\\win";
}

sub windowsOutputDir()
{
    return windowsSourceDir() . "\\WebKitBuild";
}

sub setupAppleWinEnv()
{
    return unless isAppleWinWebKit();

    if (isWindowsNT()) {
        my $restartNeeded = 0;
        my %variablesToSet = ();

        # Setting the environment variable 'CYGWIN' to 'tty' makes cygwin enable extra support (i.e., termios)
        # for UNIX-like ttys in the Windows console
        $variablesToSet{CYGWIN} = "tty" unless $ENV{CYGWIN};
        
        # Those environment variables must be set to be able to build inside Visual Studio.
        $variablesToSet{WEBKITLIBRARIESDIR} = windowsLibrariesDir() unless $ENV{WEBKITLIBRARIESDIR};
        $variablesToSet{WEBKITOUTPUTDIR} = windowsOutputDir() unless $ENV{WEBKITOUTPUTDIR};

        foreach my $variable (keys %variablesToSet) {
            print "Setting the Environment Variable '" . $variable . "' to '" . $variablesToSet{$variable} . "'\n\n";
            system qw(regtool -s set), '\\HKEY_CURRENT_USER\\Environment\\' . $variable, $variablesToSet{$variable};
            $restartNeeded ||= $variable eq "WEBKITLIBRARIESDIR" || $variable eq "WEBKITOUTPUTDIR";
        }

        if ($restartNeeded) {
            print "Please restart your computer before attempting to build inside Visual Studio.\n\n";
        }
    } else {
        if (!$ENV{'WEBKITLIBRARIESDIR'}) {
            print "Warning: You must set the 'WebKitLibrariesDir' environment variable\n";
            print "         to be able build WebKit from within Visual Studio.\n";
            print "         Make sure that 'WebKitLibrariesDir' points to the\n";
            print "         'WebKitLibraries/win' directory, not the 'WebKitLibraries/' directory.\n\n";
        }
        if (!$ENV{'WEBKITOUTPUTDIR'}) {
            print "Warning: You must set the 'WebKitOutputDir' environment variable\n";
            print "         to be able build WebKit from within Visual Studio.\n\n";
        }
    }
}

sub setupCygwinEnv()
{
    return if !isCygwin();
    return if $vcBuildPath;

    my $vsInstallDir;
    my $programFilesPath = $ENV{'PROGRAMFILES'} || "C:\\Program Files";
    if ($ENV{'VSINSTALLDIR'}) {
        $vsInstallDir = $ENV{'VSINSTALLDIR'};
    } else {
        $vsInstallDir = "$programFilesPath/Microsoft Visual Studio 8";
    }
    $vsInstallDir = `cygpath "$vsInstallDir"`;
    chomp $vsInstallDir;
    $vcBuildPath = "$vsInstallDir/Common7/IDE/devenv.com";
    if (-e $vcBuildPath) {
        # Visual Studio is installed; we can use pdevenv to build.
        $vcBuildPath = File::Spec->catfile(sourceDir(), qw(WebKitTools Scripts pdevenv));
    } else {
        # Visual Studio not found, try VC++ Express
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
        $willUseVCExpressWhenBuilding = 1;
    }

    my $qtSDKPath = "$programFilesPath/QuickTime SDK";
    if (0 && ! -e $qtSDKPath) {
        print "*************************************************************\n";
        print "Cannot find '$qtSDKPath'\n";
        print "Please download the QuickTime SDK for Windows from\n";
        print "http://developer.apple.com/quicktime/download/\n";
        print "*************************************************************\n";
        die;
    }
    
    chomp($ENV{'WEBKITLIBRARIESDIR'} = `cygpath -wa "$sourceDir/WebKitLibraries/win"`) unless $ENV{'WEBKITLIBRARIESDIR'};

    $windowsTmpPath = `cygpath -w /tmp`;
    chomp $windowsTmpPath;
    print "Building results into: ", baseProductDir(), "\n";
    print "WEBKITOUTPUTDIR is set to: ", $ENV{"WEBKITOUTPUTDIR"}, "\n";
    print "WEBKITLIBRARIESDIR is set to: ", $ENV{"WEBKITLIBRARIESDIR"}, "\n";
}

sub dieIfWindowsPlatformSDKNotInstalled
{
    my $registry32Path = "/proc/registry/";
    my $registry64Path = "/proc/registry64/";
    my $windowsPlatformSDKRegistryEntry = "HKEY_LOCAL_MACHINE/SOFTWARE/Microsoft/MicrosoftSDK/InstalledSDKs/D2FF9F89-8AA2-4373-8A31-C838BF4DBBE1";

    # FIXME: It would be better to detect whether we are using 32- or 64-bit Windows
    # and only check the appropriate entry. But for now we just blindly check both.
    return if (-e $registry32Path . $windowsPlatformSDKRegistryEntry) || (-e $registry64Path . $windowsPlatformSDKRegistryEntry);

    print "*************************************************************\n";
    print "Cannot find registry entry '$windowsPlatformSDKRegistryEntry'.\n";
    print "Please download and install the Microsoft Windows Server 2003 R2\n";
    print "Platform SDK from <http://www.microsoft.com/downloads/details.aspx?\n";
    print "familyid=0baf2b35-c656-4969-ace8-e4c0c0716adb&displaylang=en>.\n\n";
    print "Then follow step 2 in the Windows section of the \"Installing Developer\n";
    print "Tools\" instructions at <http://www.webkit.org/building/tools.html>.\n";
    print "*************************************************************\n";
    die;
}

sub copyInspectorFrontendFiles
{
    my $productDir = productDir();
    my $sourceInspectorPath = sourceDir() . "/WebCore/inspector/front-end/";
    my $inspectorResourcesDirPath = $ENV{"WEBKITINSPECTORRESOURCESDIR"};

    if (!defined($inspectorResourcesDirPath)) {
        $inspectorResourcesDirPath = "";
    }

    if (isAppleMacWebKit()) {
        $inspectorResourcesDirPath = $productDir . "/WebCore.framework/Resources/inspector";
    } elsif (isAppleWinWebKit()) {
        $inspectorResourcesDirPath = $productDir . "/WebKit.resources/inspector";
    } elsif (isQt() || isGtk()) {
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
    return system "rsync", "-aut", "--exclude=/.DS_Store", "--exclude=*.re2js", "--exclude=.svn/", !isQt() ? "--exclude=/WebKit.qrc" : "", $sourceInspectorPath, $inspectorResourcesDirPath;
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

sub buildVisualStudioProject
{
    my ($project, $clean) = @_;
    setupCygwinEnv();

    my $config = configurationForVisualStudio();

    dieIfWindowsPlatformSDKNotInstalled() if $willUseVCExpressWhenBuilding;

    chomp(my $winProjectPath = `cygpath -w "$project"`);
    
    my $action = "/build";
    if ($clean) {
        $action = "/clean";
    }

    my @command = ($vcBuildPath, $winProjectPath, $action, $config);

    print join(" ", @command), "\n";
    return system @command;
}

sub downloadWafIfNeeded
{
    # get / update waf if needed
    my $waf = "$sourceDir/WebKitTools/wx/waf";
    my $wafURL = 'http://wxwebkit.wxcommunity.com/downloads/deps/waf';
    if (!-f $waf) {
        my $result = system "curl -o $waf $wafURL";
        chmod 0755, $waf;
    }
}

sub buildWafProject
{
    my ($project, $shouldClean, @options) = @_;
    
    # set the PYTHONPATH for waf
    my $pythonPath = $ENV{'PYTHONPATH'};
    if (!defined($pythonPath)) {
        $pythonPath = '';
    }
    my $sourceDir = sourceDir();
    my $newPythonPath = "$sourceDir/WebKitTools/wx/build:$pythonPath";
    if (isCygwin()) {
        $newPythonPath = `cygpath --mixed --path $newPythonPath`;
    }
    $ENV{'PYTHONPATH'} = $newPythonPath;
    
    print "Building $project\n";

    my $wafCommand = "$sourceDir/WebKitTools/wx/waf";
    if ($ENV{'WXWEBKIT_WAF'}) {
        $wafCommand = $ENV{'WXWEBKIT_WAF'};
    }
    if (isCygwin()) {
        $wafCommand = `cygpath --windows "$wafCommand"`;
        chomp($wafCommand);
    }
    if ($shouldClean) {
        return system $wafCommand, "clean", "distclean";
    }
    
    return system $wafCommand, 'configure', 'build', 'install', @options;
}

sub retrieveQMakespecVar
{
    my $mkspec = $_[0];
    my $varname = $_[1];

    my $varvalue = undef;
    #print "retrieveMakespecVar " . $mkspec . ", " . $varname . "\n";

    local *SPEC;
    open SPEC, "<$mkspec" or return $varvalue;
    while (<SPEC>) {
        if ($_ =~ /\s*include\((.+)\)/) {
            # open the included mkspec
            my $oldcwd = getcwd();
            (my $volume, my $directories, my $file) = File::Spec->splitpath($mkspec);
            my $newcwd = "$volume$directories";
            chdir $newcwd if $newcwd;
            $varvalue = retrieveQMakespecVar($1, $varname);
            chdir $oldcwd;
        } elsif ($_ =~ /$varname\s*=\s*([^\s]+)/) {
            $varvalue = $1;
            last;
        }
    }
    close SPEC;
    return $varvalue;
}

sub qtMakeCommand($)
{
    my ($qmakebin) = @_;
    chomp(my $mkspec = `$qmakebin -query QMAKE_MKSPECS`);
    $mkspec .= "/default";
    my $compiler = retrieveQMakespecVar("$mkspec/qmake.conf", "QMAKE_CC");

    #print "default spec: " . $mkspec . "\n";
    #print "compiler found: " . $compiler . "\n";

    if ($compiler && $compiler eq "cl") {
        return "nmake";
    }

    return "make";
}

sub autotoolsFlag($$)
{
    my ($flag, $feature) = @_;
    my $prefix = $flag ? "--enable" : "--disable";

    return $prefix . '-' . $feature;
}

sub autogenArgumentsHaveChanged($@)
{
    my ($filename, @currentArguments) = @_;

    if (! -e $filename) {
        return 1;
    }

    open(AUTOTOOLS_ARGUMENTS, $filename);
    chomp(my $previousArguments = <AUTOTOOLS_ARGUMENTS>);
    close(AUTOTOOLS_ARGUMENTS);

    return $previousArguments ne join(" ", @currentArguments);
}

sub buildAutotoolsProject($@)
{
    my ($clean, @buildParams) = @_;

    my $make = 'make';
    my $dir = productDir();
    my $config = passedConfiguration() || configuration();
    my $prefix;

    my @buildArgs = ();
    my $makeArgs = $ENV{"WebKitMakeArguments"} || "";
    for my $i (0 .. $#buildParams) {
        my $opt = $buildParams[$i];
        if ($opt =~ /^--makeargs=(.*)/i ) {
            $makeArgs = $makeArgs . " " . $1;
        } elsif ($opt =~ /^--prefix=(.*)/i ) {
            $prefix = $1;
        } else {
            push @buildArgs, $opt;
        }
    }

    # Automatically determine the number of CPUs for make only
    # if make arguments haven't already been specified.
    if ($makeArgs eq "") {
        $makeArgs = "-j" . numberOfCPUs();
    }

    $prefix = $ENV{"WebKitInstallationPrefix"} if !defined($prefix);
    push @buildArgs, "--prefix=" . $prefix if defined($prefix);

    # check if configuration is Debug
    if ($config =~ m/debug/i) {
        push @buildArgs, "--enable-debug";
    } else {
        push @buildArgs, "--disable-debug";
    }

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

    # If GNUmakefile exists, don't run autogen.sh. The makefile should be
    # smart enough to track autotools dependencies and re-run autogen.sh
    # when build files change.
    my $autogenArgumentsFile = "previous-autogen-arguments.txt";
    my $result;
    if (!(-e "GNUmakefile") or autogenArgumentsHaveChanged($autogenArgumentsFile, @buildArgs)) {

        # Write autogen.sh arguments to a file so that we can detect
        # when they change and automatically re-run it.
        open(AUTOTOOLS_ARGUMENTS, ">$autogenArgumentsFile");
        print AUTOTOOLS_ARGUMENTS  join(" ", @buildArgs);
        close(AUTOTOOLS_ARGUMENTS);

        print "Calling configure in " . $dir . "\n\n";
        print "Installation prefix directory: $prefix\n" if(defined($prefix));

        # Make the path relative since it will appear in all -I compiler flags.
        # Long argument lists cause bizarre slowdowns in libtool.
        my $relSourceDir = File::Spec->abs2rel($sourceDir) || ".";
        $result = system "$relSourceDir/autogen.sh", @buildArgs;
        if ($result ne 0) {
            die "Failed to setup build environment using 'autotools'!\n";
        }
    }

    $result = system "$make $makeArgs";
    if ($result ne 0) {
        die "\nFailed to build WebKit using '$make'!\n";
    }

    chdir ".." or die;
    return $result;
}

sub buildCMakeProject($@)
{
    my ($port, $clean, @buildParams) = @_;
    my $dir = File::Spec->canonpath(baseProductDir());
    my $config = configuration();
    my $result;
    my $makeArgs = "";
    my @buildArgs;
    
    $makeArgs .= " -j" . numberOfCPUs() if ($makeArgs !~ m/-j\s*\d+/);

    if ($clean) {
        print "Cleaning the build directory '$dir'\n";
        $dir = File::Spec->catfile($dir, $config);
        File::Path::remove_tree($dir, {keep_root => 1});
        $result = 0;
    } else {
        my $cmakebin = "cmake";
        my $make = "make";

        push @buildArgs, "-DPORT=$port";

        for my $i (0 .. $#buildParams) {
            my $opt = $buildParams[$i];
            if ($opt =~ /^--makeargs=(.*)/i ) {
                $makeArgs = $1;
            } elsif ($opt =~ /^--prefix=(.*)/i ) {
                push @buildArgs, "-DCMAKE_INSTALL_PREFIX=$1";
            } else {
                push @buildArgs, $opt;
            }
        }

        if ($config =~ m/debug/i) {
            push @buildArgs, "-DCMAKE_BUILD_TYPE=Debug";
        } elsif ($config =~ m/release/i) {
            push @buildArgs, "-DCMAKE_BUILD_TYPE=Release";
        }

        push @buildArgs, sourceDir();

        $dir = File::Spec->catfile($dir, $config);
        File::Path::mkpath($dir);
        chdir $dir or die "Failed to cd into " . $dir . "\n";
        
        print "Calling '$cmakebin @buildArgs' in " . $dir . "\n\n";
        my $result = system "$cmakebin @buildArgs";
        if ($result ne 0) {
            die "Failed while running $cmakebin to generate makefiles!\n";
        }

        print "Calling '$make $makeArgs' in " . $dir . "\n\n";
        $result = system "$make $makeArgs";

        chdir ".." or die;
    }

    return $result; 
}

sub buildCMakeEflProject($@)
{
    my ($clean, @buildArgs) = @_;
    return buildCMakeProject("Efl", $clean, @buildArgs);
}

sub buildQMakeProject($@)
{
    my ($clean, @buildParams) = @_;

    my @buildArgs = ("-r");

    my $qmakebin = "qmake"; # Allow override of the qmake binary from $PATH
    my $makeargs = "";
    my $installHeaders;
    my $installLibs;
    for my $i (0 .. $#buildParams) {
        my $opt = $buildParams[$i];
        if ($opt =~ /^--qmake=(.*)/i ) {
            $qmakebin = $1;
        } elsif ($opt =~ /^--qmakearg=(.*)/i ) {
            push @buildArgs, $1;
        } elsif ($opt =~ /^--makeargs=(.*)/i ) {
            $makeargs = $1;
        } elsif ($opt =~ /^--install-headers=(.*)/i ) {
            $installHeaders = $1;
        } elsif ($opt =~ /^--install-libs=(.*)/i ) {
            $installLibs = $1;
        } else {
            push @buildArgs, $opt;
        }
    }

    my $make = qtMakeCommand($qmakebin);
    my $config = configuration();
    push @buildArgs, "INSTALL_HEADERS=" . $installHeaders if defined($installHeaders);
    push @buildArgs, "INSTALL_LIBS=" . $installLibs if defined($installLibs);
    my $dir = File::Spec->canonpath(baseProductDir());
    $dir = File::Spec->catfile($dir, $config) unless isSymbian();
    File::Path::mkpath($dir);
    chdir $dir or die "Failed to cd into " . $dir . "\n";

    print "Generating derived sources\n\n";

    push @buildArgs, "OUTPUT_DIR=" . baseProductDir() . "/$config";

    my @dsQmakeArgs = @buildArgs;
    push @dsQmakeArgs, "-r";
    push @dsQmakeArgs, sourceDir() . "/DerivedSources.pro";
    push @dsQmakeArgs, "-o Makefile.DerivedSources";
    print "Calling '$qmakebin @dsQmakeArgs' in " . $dir . "\n\n";
    my $result = system "$qmakebin @dsQmakeArgs";
    if ($result ne 0) {
        die "Failed while running $qmakebin to generate derived sources!\n";
    }

    my $dsMakefile = "Makefile.DerivedSources";

    # Iterate over different source directories manually to workaround a problem with qmake+extraTargets+s60
    my @subdirs = ("JavaScriptCore", "WebCore", "WebKit/qt/Api");
    if (grep { $_ eq "CONFIG+=webkit2"} @buildArgs) {
        push @subdirs, "WebKit2";
    }

    for my $subdir (@subdirs) {
        print "Calling '$make $makeargs -f $dsMakefile generated_files' in " . $dir . "/$subdir\n\n";
        if ($make eq "nmake") {
            my $subdirWindows = $subdir;
            $subdirWindows =~ s:/:\\:g;
            $result = system "pushd $subdirWindows && $make $makeargs -f $dsMakefile generated_files && popd";
        } else {
            $result = system "$make $makeargs -C $subdir -f $dsMakefile generated_files";
        }
        if ($result ne 0) {
            die "Failed to generate ${subdir}'s derived sources!\n";
        }
    }

    if ($config =~ m/debug/i) {
        push @buildArgs, "CONFIG-=release";
        push @buildArgs, "CONFIG+=debug";
    } else {
        my $passedConfig = passedConfiguration() || "";
        if (!isDarwin() || $passedConfig =~ m/release/i) {
            push @buildArgs, "CONFIG+=release";
            push @buildArgs, "CONFIG-=debug";
        } else {
            push @buildArgs, "CONFIG+=debug";
            push @buildArgs, "CONFIG+=debug_and_release";
        }
    }

    push @buildArgs, sourceDir() . "/WebKit.pro";

    print "Calling '$qmakebin @buildArgs' in " . $dir . "\n\n";
    print "Installation headers directory: $installHeaders\n" if(defined($installHeaders));
    print "Installation libraries directory: $installLibs\n" if(defined($installLibs));

    $result = system "$qmakebin @buildArgs";
    if ($result ne 0) {
       die "Failed to setup build environment using $qmakebin!\n";
    }

    # Manually create makefiles for the examples so we don't build by default
    my $examplesDir = $dir . "/WebKit/qt/examples";
    File::Path::mkpath($examplesDir);
    $buildArgs[-1] = sourceDir() . "/WebKit/qt/examples/examples.pro";
    chdir $examplesDir or die;
    print "Calling '$qmakebin @buildArgs' in " . $examplesDir . "\n\n";
    $result = system "$qmakebin @buildArgs";
    die "Failed to create makefiles for the examples!\n" if $result ne 0;
    chdir $dir or die;

    if ($clean) {
      print "Calling '$make $makeargs distclean' in " . $dir . "\n\n";
      $result = system "$make $makeargs distclean";
    } elsif (isSymbian()) {
      print "\n\nWebKit is now configured for building, but you have to make\n";
      print "a choice about the target yourself. To start the build run:\n\n";
      print "    make release-armv5|debug-winscw|etc.\n\n";
    } else {
      print "Calling '$make $makeargs' in " . $dir . "\n\n";
      $result = system "$make $makeargs";
    }

    chdir ".." or die;
    return $result;
}

sub buildQMakeQtProject($$@)
{
    my ($project, $clean, @buildArgs) = @_;

    return buildQMakeProject($clean, @buildArgs);
}

sub buildGtkProject($$@)
{
    my ($project, $clean, @buildArgs) = @_;

    if ($project ne "WebKit") {
        die "The Gtk port builds JavaScriptCore, WebCore and WebKit in one shot! Only call it for 'WebKit'.\n";
    }

    return buildAutotoolsProject($clean, @buildArgs);
}

sub buildChromiumMakefile($$)
{
    my ($target, $clean) = @_;
    if ($clean) {
        return system qw(rm -rf out);
    }
    my $config = configuration();
    my $numCpus = numberOfCPUs();
    my @command = ("make", "-fMakefile.chromium", "-j$numCpus", "BUILDTYPE=$config", $target);
    print join(" ", @command) . "\n";
    return system @command;
}

sub buildChromiumVisualStudioProject($$)
{
    my ($projectPath, $clean) = @_;

    my $config = configuration();
    my $action = "/build";
    $action = "/clean" if $clean;

    # Find Visual Studio installation.
    my $vsInstallDir;
    my $programFilesPath = $ENV{'PROGRAMFILES'} || "C:\\Program Files";
    if ($ENV{'VSINSTALLDIR'}) {
        $vsInstallDir = $ENV{'VSINSTALLDIR'};
    } else {
        $vsInstallDir = "$programFilesPath/Microsoft Visual Studio 8";
    }
    $vsInstallDir = `cygpath "$vsInstallDir"` if isCygwin();
    chomp $vsInstallDir;
    $vcBuildPath = "$vsInstallDir/Common7/IDE/devenv.com";
    if (! -e $vcBuildPath) {
        # Visual Studio not found, try VC++ Express
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

    # Create command line and execute it.
    my @command = ($vcBuildPath, $projectPath, $action, $config);
    print "Building results into: ", baseProductDir(), "\n";
    print join(" ", @command), "\n";
    return system @command;
}

sub buildChromium($@)
{
    my ($clean, @options) = @_;

    # We might need to update DEPS or re-run GYP if things have changed.
    system("perl", "WebKitTools/Scripts/update-webkit-chromium") == 0 or die $!;

    my $result = 1;
    if (isDarwin()) {
        # Mac build - builds the root xcode project.
        $result = buildXCodeProject("WebKit/chromium/WebKit", $clean, "-configuration", configuration(), @options);
    } elsif (isCygwin() || isWindows()) {
        # Windows build - builds the root visual studio solution.
        $result = buildChromiumVisualStudioProject("WebKit/chromium/WebKit.sln", $clean);
    } elsif (isLinux()) {
        # Linux build - build using make.
        $ result = buildChromiumMakefile("all", $clean);
    } else {
        print STDERR "This platform is not supported by chromium.\n";
    }
    return $result;
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
        $env->{PATH} = join(':', productDir(), dirname(installedSafariPath()), appleApplicationSupportPath(), $env->{PATH} || "");
    } elsif (isQt()) {
        my $qtLibs = `qmake -query QT_INSTALL_LIBS`;
        $qtLibs =~ s/[\n|\r]$//g;
        $env->{PATH} = join(';', $qtLibs, productDir() . "/lib", $env->{PATH} || "");
    }
}

sub runSafari
{
    my ($debugger) = @_;

    if (isAppleMacWebKit()) {
        return system "$FindBin::Bin/gdb-safari", argumentsForConfiguration() if $debugger;

        my $productDir = productDir();
        print "Starting Safari with DYLD_FRAMEWORK_PATH set to point to built WebKit in $productDir.\n";
        $ENV{DYLD_FRAMEWORK_PATH} = $productDir;
        $ENV{WEBKIT_UNSET_DYLD_FRAMEWORK_PATH} = "YES";
        if (!isTiger() && architecture()) {
            return system "arch", "-" . architecture(), safariPath(), @ARGV;
        } else {
            return system safariPath(), @ARGV;
        }
    }

    if (isAppleWinWebKit()) {
        my $result;
        my $productDir = productDir();
        if ($debugger) {
            setupCygwinEnv();
            chomp($ENV{WEBKITNIGHTLY} = `cygpath -wa "$productDir"`);
            my $safariPath = safariPath();
            chomp($safariPath = `cygpath -wa "$safariPath"`);
            $result = system $vcBuildPath, "/debugexe", "\"$safariPath\"", @ARGV;
        } else {
            $result = system File::Spec->catfile(productDir(), "WebKit.exe"), @ARGV;
        }
        return $result if $result;
    }

    return 1;
}

sub runMiniBrowser
{
    if (isAppleMacWebKit()) {
        my $productDir = productDir();
        print "Starting MiniBrowser with DYLD_FRAMEWORK_PATH set to point to $productDir.\n";
        $ENV{DYLD_FRAMEWORK_PATH} = $productDir;
        $ENV{WEBKIT_UNSET_DYLD_FRAMEWORK_PATH} = "YES";
        my $miniBrowserPath = "$productDir/MiniBrowser.app/Contents/MacOS/MiniBrowser";
        if (!isTiger() && architecture()) {
            return system "arch", "-" . architecture(), $miniBrowserPath, @ARGV;
        } else {
            return system $miniBrowserPath, @ARGV;
        }
    }

    return 1;
}

sub debugMiniBrowser
{
    if (isAppleMacWebKit()) {
        my $gdbPath = "/usr/bin/gdb";
        die "Can't find gdb executable. Is gdb installed?\n" unless -x $gdbPath;

        my $productDir = productDir();

        $ENV{DYLD_FRAMEWORK_PATH} = $productDir;
        $ENV{WEBKIT_UNSET_DYLD_FRAMEWORK_PATH} = 'YES';

        my $miniBrowserPath = "$productDir/MiniBrowser.app/Contents/MacOS/MiniBrowser";

        print "Starting MiniBrowser under gdb with DYLD_FRAMEWORK_PATH set to point to built WebKit2 in $productDir.\n";
        my @architectureFlags = ("-arch", architecture()) if !isTiger();
        exec $gdbPath, @architectureFlags, $miniBrowserPath or die;
        return;
    }
    
    return 1;
}

sub runWebKitTestRunner
{
    if (isAppleMacWebKit()) {
        my $productDir = productDir();
        print "Starting WebKitTestRunner with DYLD_FRAMEWORK_PATH set to point to $productDir.\n";
        $ENV{DYLD_FRAMEWORK_PATH} = $productDir;
        $ENV{WEBKIT_UNSET_DYLD_FRAMEWORK_PATH} = "YES";
        my $webKitTestRunnerPath = "$productDir/WebKitTestRunner";
        if (!isTiger() && architecture()) {
            return system "arch", "-" . architecture(), $webKitTestRunnerPath, @ARGV;
        } else {
            return system $webKitTestRunnerPath, @ARGV;
        }
    }

    return 1;
}

sub debugWebKitTestRunner
{
    if (isAppleMacWebKit()) {
        my $gdbPath = "/usr/bin/gdb";
        die "Can't find gdb executable. Is gdb installed?\n" unless -x $gdbPath;

        my $productDir = productDir();
        $ENV{DYLD_FRAMEWORK_PATH} = $productDir;
        $ENV{WEBKIT_UNSET_DYLD_FRAMEWORK_PATH} = 'YES';

        my $webKitTestRunnerPath = "$productDir/WebKitTestRunner";

        print "Starting WebKitTestRunner under gdb with DYLD_FRAMEWORK_PATH set to point to $productDir.\n";
        my @architectureFlags = ("-arch", architecture()) if !isTiger();
        exec $gdbPath, @architectureFlags, $webKitTestRunnerPath or die;
        return;
    }

    return 1;
}

1;
