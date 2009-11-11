# Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
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
my $baseProductDir;
my @baseProductDirOption;
my $configuration;
my $configurationForVisualStudio;
my $configurationProductDir;
my $sourceDir;
my $currentSVNRevision;
my $osXVersion;
my $isQt;
my %qtFeatureDefaults;
my $isGtk;
my $isWx;
my @wxArgs;
my $isChromium;

# Variables for Win32 support
my $vcBuildPath;
my $windowsTmpPath;

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

    if (isAppleMacWebKit()) {
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
    }

    if (!defined($baseProductDir)) { # Port-spesific checks failed, use default
        $baseProductDir = $ENV{"WEBKITOUTPUTDIR"} || "$sourceDir/WebKitBuild";
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

sub jscPath($)
{
    my ($productDir) = @_;
    my $jscName = "jsc";
    $jscName .= "_debug"  if (isCygwin() && ($configuration eq "Debug"));
    return "$productDir/$jscName";
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
    push(@args, '--gtk') if isGtk();
    push(@args, '--wx') if isWx();
    push(@args, '--chromium') if isChromium();
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
        # [Gtk] We don't have Release/Debug configurations in straight
        # autotool builds (non build-webkit). In this case and if
        # WEBKITOUTPUTDIR exist, use that as our configuration dir. This will
        # allows us to run run-webkit-tests without using build-webkit.
        if ($ENV{"WEBKITOUTPUTDIR"} && isGtk()) {
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
    $productDir .= "/Programs" if isGtk();

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
    push @coverageSupportOptions, "OTHER_LDFLAGS=\$(OTHER_LDFLAGS) -ftest-coverage -fprofile-arcs -framework AppKit";
    return @coverageSupportOptions;
}

my $passedConfiguration;
my $searchedForPassedConfiguration;
sub determinePassedConfiguration
{
    return if $searchedForPassedConfiguration;
    $searchedForPassedConfiguration = 1;

    my $isWinCairo = checkForArgumentAndRemoveFromARGV("--cairo-win32");

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
        } elsif (isWindows() or isCygwin()) {
            return "$configurationProductDir/lib/$libraryName.dll";
        } else {
            return "$configurationProductDir/lib/lib$libraryName.so";
        }
    }
    if (isWx()) {
        return "$configurationProductDir/libwxwebkit.dylib";
    }
    if (isGtk()) {
        return "$configurationProductDir/$libraryName/../.libs/libwebkit-1.0.so";
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

    die "Unsupported platform, can't determine built library locations.";
}

# Check to see that all the frameworks are built.
sub checkFrameworks
{
    return if isCygwin();
    my @frameworks = ("JavaScriptCore", "WebCore");
    push(@frameworks, "WebKit") if isAppleMacWebKit();
    for my $framework (@frameworks) {
        my $path = builtDylibPathForName($framework);
        die "Can't find built framework at \"$path\".\n" unless -x $path;
    }
}

sub libraryContainsSymbol
{
    my $path = shift;
    my $symbol = shift;

    if (isCygwin() or isWindows()) {
        # FIXME: Implement this for Windows.
        return 0;
    }

    my $foundSymbol = 0;
    if (-e $path) {
        open NM, "-|", "nm", $path or die;
        while (<NM>) {
            $foundSymbol = 1 if /$symbol/;
        }
        close NM;
    }
    return $foundSymbol;
}

sub hasMathMLSupport
{
    my $path = shift;

    return libraryContainsSymbol($path, "MathMLElement");
}

sub checkWebCoreMathMLSupport
{
    my $required = shift;
    my $framework = "WebCore";
    my $path = builtDylibPathForName($framework);
    my $hasMathML = hasMathMLSupport($path);
    if ($required && !$hasMathML) {
        die "$framework at \"$path\" does not include MathML Support, please run build-webkit --mathml\n";
    }
    return $hasMathML;
}

sub hasSVGSupport
{
    my $path = shift;

    if (isQt()) {
        return 1;
    }
    
    if (isWx()) {
        return 0;
    }

    return libraryContainsSymbol($path, "SVGElement");
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

sub hasAcceleratedCompositingSupport
{
    return 0 if isCygwin() || isQt();

    my $path = shift;
    return libraryContainsSymbol($path, "GraphicsLayer");
}

sub checkWebCoreAcceleratedCompositingSupport
{
    my $required = shift;
    my $framework = "WebCore";
    my $path = builtDylibPathForName($framework);
    my $hasAcceleratedCompositing = hasAcceleratedCompositingSupport($path);
    if ($required && !$hasAcceleratedCompositing) {
        die "$framework at \"$path\" does not use accelerated compositing\n";
    }
    return $hasAcceleratedCompositing;
}

sub has3DRenderingSupport
{
    return 0 if isQt();

    my $path = shift;
    return libraryContainsSymbol($path, "WebCoreHas3DRendering");
}

sub checkWebCore3DRenderingSupport
{
    my $required = shift;
    my $framework = "WebCore";
    my $path = builtDylibPathForName($framework);
    my $has3DRendering = has3DRenderingSupport($path);
    if ($required && !$has3DRendering) {
        die "$framework at \"$path\" does not include 3D rendering Support, please run build-webkit --3d-rendering\n";
    }
    return $has3DRendering;
}

sub has3DCanvasSupport
{
    return 0 if isQt();

    my $path = shift;
    return libraryContainsSymbol($path, "WebGLShader");
}

sub checkWebCore3DCanvasSupport
{
    my $required = shift;
    my $framework = "WebCore";
    my $path = builtDylibPathForName($framework);
    my $has3DCanvas = has3DCanvasSupport($path);
    if ($required && !$has3DCanvas) {
        die "$framework at \"$path\" does not include 3D Canvas Support, please run build-webkit --3d-canvas\n";
    }
    return $has3DCanvas;
}

sub hasWMLSupport
{
    my $path = shift;
    return libraryContainsSymbol($path, "WMLElement");
}

sub removeLibraryDependingOnWML
{
    my $frameworkName = shift;
    my $shouldHaveWML = shift;

    my $path = builtDylibPathForName($frameworkName);
    return unless -x $path;

    my $hasWML = hasWMLSupport($path);
    system "rm -f $path" if ($shouldHaveWML xor $hasWML);
}

sub checkWebCoreWMLSupport
{
    my $required = shift;
    my $framework = "WebCore";
    my $path = builtDylibPathForName($framework);
    my $hasWML = hasWMLSupport($path);
    if ($required && !$hasWML) {
        die "$framework at \"$path\" does not include WML Support, please run build-webkit --wml\n";
    }
    return $hasWML;
}

sub hasXHTMLMPSupport
{
    my $path = shift;
    return libraryContainsSymbol($path, "isXHTMLMPDocument");
}

sub checkWebCoreXHTMLMPSupport
{
    my $required = shift;
    my $framework = "WebCore";
    my $path = builtDylibPathForName($framework);
    my $hasXHTMLMP = hasXHTMLMPSupport($path);
    if ($required && !$hasXHTMLMP) {
        die "$framework at \"$path\" does not include XHTML MP Support\n";
    }
    return $hasXHTMLMP;
}

sub hasWCSSSupport
{
    # FIXME: When WCSS support is landed this should be updated to check for WCSS
    # being enabled in a manner similar to how we check for XHTML MP above.
    return 0;
}

sub checkWebCoreWCSSSupport
{
    my $required = shift;
    my $framework = "WebCore";
    my $path = builtDylibPathForName($framework);
    my $hasWCSS = hasWCSSSupport($path);
    if ($required && !$hasWCSS) {
        die "$framework at \"$path\" does not include WCSS Support\n";
    }
    return $hasWCSS;
}

sub isQt()
{
    determineIsQt();
    return $isQt;
}

sub qtFeatureDefaults()
{
    determineQtFeatureDefaults();
    return %qtFeatureDefaults;
}

sub determineQtFeatureDefaults()
{
    return if %qtFeatureDefaults;
    my $originalCwd = getcwd();
    chdir File::Spec->catfile(sourceDir(), "WebCore");
    my $defaults = `qmake CONFIG+=compute_defaults 2>&1`;
    chdir $originalCwd;

    while ($defaults =~ m/(\S*?)=(.*?)( |$)/gi) {
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

    # The presence of QTDIR only means Qt if --gtk is not on the command-line
    if (isGtk() || isWx()) {
        $isQt = 0;
        return;
    }
    
    $isQt = defined($ENV{'QTDIR'});
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

sub isAppleWebKit()
{
    return !(isQt() or isGtk() or isWx() or isChromium());
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
    if (isGtk() || isQt() || isWx()) {
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
        return "QtLauncher";
    } elsif (isWx()) {
        return "wxBrowser";
    } elsif (isAppleWebKit()) {
        return "Safari";
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
    } elsif (isGtk() or isQt() or isWx() or isChromium()) {
        my @cmds = qw(flex bison gperf);
        my @missing = ();
        foreach my $cmd (@cmds) {
            if (not `$cmd --version`) {
                push @missing, $cmd;
            }
        }
        if (@missing) {
            my $list = join ", ", @missing;
            die "ERROR: $list missing but required to build WebKit.\n";
        }
    }
    # Win32 and other platforms may want to check for minimum config
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

    chomp(my $winProjectPath = `cygpath -w "$project"`);
    
    my $action = "/build";
    if ($clean) {
        $action = "/clean";
    }

    my $useenv = "/useenv";
    if (isChromium()) {
        $useenv = "";
    }

    my @command = ($vcBuildPath, $useenv, $winProjectPath, $action, $config);

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

    my $compiler = "unknown";
    #print "retrieveMakespecVar " . $mkspec . ", " . $varname . "\n";

    local *SPEC;
    open SPEC, "<$mkspec" or return "make";
    while (<SPEC>) {
        if ($_ =~ /\s*include\((.+)\)/) {
            # open the included mkspec
            my $oldcwd = getcwd();
            (my $volume, my $directories, my $file) = File::Spec->splitpath($mkspec);
            my $newcwd = "$volume$directories";
            chdir $newcwd if $newcwd;
            $compiler = retrieveQMakespecVar($1, $varname);
            chdir $oldcwd;
        } elsif ($_ =~ /$varname\s*=\s*([^\s]+)/) {
            $compiler = $1;
            last;
        }
    }
    close SPEC;
    return $compiler;
}

sub qtMakeCommand($)
{
    my ($qmakebin) = @_;
    chomp(my $mkspec = `$qmakebin -query QMAKE_MKSPECS`);
    $mkspec .= "/default";
    my $compiler = retrieveQMakespecVar("$mkspec/qmake.conf", "QMAKE_CC");

    #print "default spec: " . $mkspec . "\n";
    #print "compiler found: " . $compiler . "\n";

    if ($compiler eq "cl") {
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

sub buildAutotoolsProject($@)
{
    my ($clean, @buildParams) = @_;

    my $make = 'make';
    my $dir = productDir();
    my $config = passedConfiguration() || configuration();
    my $prefix = $ENV{"WebKitInstallationPrefix"};

    my @buildArgs = ();
    my $makeArgs = $ENV{"WebKitMakeArguments"} || "";
    for my $i (0 .. $#buildParams) {
        my $opt = $buildParams[$i];
        if ($opt =~ /^--makeargs=(.*)/i ) {
            $makeArgs = $makeArgs . " " . $1;
        } else {
            push @buildArgs, $opt;
        }
    }

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
        system "mkdir", "-p", "$dir";
        if (! -d $dir) {
            die "Failed to create build directory " . $dir;
        }
    }

    chdir $dir or die "Failed to cd into " . $dir . "\n";

    my $result;
    if ($clean) {
        #$result = system $make, "distclean";
        return 0;
    }

    print "Calling configure in " . $dir . "\n\n";
    print "Installation directory: $prefix\n" if(defined($prefix));

    # Make the path relative since it will appear in all -I compiler flags.
    # Long argument lists cause bizarre slowdowns in libtool.
    my $relSourceDir = File::Spec->abs2rel($sourceDir);
    $relSourceDir = "." if !$relSourceDir;

    $result = system "$relSourceDir/autogen.sh", @buildArgs;
    if ($result ne 0) {
        die "Failed to setup build environment using 'autotools'!\n";
    }

    $result = system "$make $makeArgs";
    if ($result ne 0) {
        die "\nFailed to build WebKit using '$make'!\n";
    }

    chdir ".." or die;
    return $result;
}

sub buildQMakeProject($@)
{
    my ($clean, @buildParams) = @_;

    my @buildArgs = ("-r");

    my $qmakebin = "qmake"; # Allow override of the qmake binary from $PATH
    my $makeargs = "";
    for my $i (0 .. $#buildParams) {
        my $opt = $buildParams[$i];
        if ($opt =~ /^--qmake=(.*)/i ) {
            $qmakebin = $1;
        } elsif ($opt =~ /^--qmakearg=(.*)/i ) {
            push @buildArgs, $1;
        } elsif ($opt =~ /^--makeargs=(.*)/i ) {
            $makeargs = $1;
        } else {
            push @buildArgs, $opt;
        }
    }

    my $make = qtMakeCommand($qmakebin);
    my $config = configuration();
    my $prefix = $ENV{"WebKitInstallationPrefix"};

    push @buildArgs, "OUTPUT_DIR=" . baseProductDir() . "/$config";
    push @buildArgs, sourceDir() . "/WebKit.pro";
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

    my $dir = File::Spec->canonpath(baseProductDir());
    my @mkdirArgs;
    push @mkdirArgs, "-p" if !isWindows();
    if (! -d $dir) {
        system "mkdir", @mkdirArgs, "$dir";
        if (! -d $dir) {
            die "Failed to create product directory " . $dir;
        }
    }
    $dir = File::Spec->catfile($dir, $config);
    if (! -d $dir) {
        system "mkdir", @mkdirArgs, "$dir";
        if (! -d $dir) {
            die "Failed to create build directory " . $dir;
        }
    }

    chdir $dir or die "Failed to cd into " . $dir . "\n";

    print "Calling '$qmakebin @buildArgs' in " . $dir . "\n\n";
    print "Installation directory: $prefix\n" if(defined($prefix));

    my $result = system "$qmakebin @buildArgs";
    if ($result ne 0) {
       die "Failed to setup build environment using $qmakebin!\n";
    }

    if ($clean) {
      $result = system "$make $makeargs distclean";
    } else {
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

sub buildChromium($@)
{
    my ($clean, @options) = @_;

    my $result = 1;
    if (isDarwin()) {
        # Mac build - builds the root xcode project.
        $result = buildXCodeProject("WebKit/chromium/webkit",
                                    $clean,
                                    (@options));
    } elsif (isCygwin()) {
        # Windows build - builds the root visual studio solution.
        $result = buildVisualStudioProject("WebKit/chromium/webkit.sln",
                                           $clean);
    } elsif (isLinux()) {
        # Linux build
        # FIXME support linux.
        print STDERR "Linux build is not supported. Yet.";
    } else {
        print STDERR "This platform is not supported by chromium.";
    }
    return $result;
}

sub setPathForRunningWebKitApp
{
    my ($env) = @_;

    return unless isAppleWinWebKit();

    $env->{PATH} = join(':', productDir(), dirname(installedSafariPath()), $env->{PATH} || "");
}

sub exitStatus($)
{
    my ($returnvalue) = @_;
    if ($^O eq "MSWin32") {
        return $returnvalue >> 8;
    }
    return WEXITSTATUS($returnvalue);
}

sub runSafari
{
    my ($debugger) = @_;

    if (isAppleMacWebKit()) {
        return system "$FindBin::Bin/gdb-safari", @ARGV if $debugger;

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
        my $script = "run-webkit-nightly.cmd";
        my $result = system "cp", "$FindBin::Bin/$script", productDir();
        return $result if $result;

        my $cwd = getcwd();
        chdir productDir();

        my $debuggerFlag = $debugger ? "/debugger" : "";
        $result = system "cmd", "/c", "call $script $debuggerFlag";
        chdir $cwd;
        return $result;
    }

    return 1;
}

1;
