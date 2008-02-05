# Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
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
use File::Basename;
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

my $baseProductDir;
my @baseProductDirOption;
my $configuration;
my $configurationForVisualStudio;
my $configurationProductDir;
my $sourceDir;
my $currentSVNRevision;
my $osXVersion;
my $isQt;
my $isGtk;
my $isWx;

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
        open PRODUCT, "defaults read com.apple.Xcode PBXApplicationwideBuildSettings 2> /dev/null |" or die;
        $baseProductDir = join '', <PRODUCT>;
        close PRODUCT;

        $baseProductDir = $1 if $baseProductDir =~ /SYMROOT\s*=\s*\"(.*?)\";/s;
        undef $baseProductDir unless $baseProductDir =~ /^\//;

        if (!defined($baseProductDir)) {
            open PRODUCT, "defaults read com.apple.Xcode PBXProductDirectory 2> /dev/null |" or die;
            $baseProductDir = <PRODUCT>;
            close PRODUCT;
            if ($baseProductDir) {
                chomp $baseProductDir;
                undef $baseProductDir unless $baseProductDir =~ /^\//;
            }
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

        if (isGit() && isGitBranchBuild()) {
            my $branch = gitBranch();
            $baseProductDir = "$baseProductDir/$branch";
        }

        @baseProductDirOption = ("SYMROOT=$baseProductDir", "OBJROOT=$baseProductDir") if (isOSX());
        if (isCygwin()) {
            my $dosBuildPath = `cygpath --windows \"$baseProductDir\"`;
            chomp $dosBuildPath;
            $ENV{"WEBKITOUTPUTDIR"} = $dosBuildPath;
        }
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
    if(isCygwin() && !isWx()) {
        $configurationProductDir = "$baseProductDir/bin";
    } else {
        determineConfiguration();
        $configurationProductDir = "$baseProductDir/$configuration";
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
    my $svnInfo = `LC_ALL=C svn info $sourceDir | grep Revision:`;
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
    if (my $config = shift @_) {
        $configuration = $config;
        return;
    }

    determinePassedConfiguration();
    $configuration = $passedConfiguration if $passedConfiguration;
}

sub safariPathFromSafariBundle
{
    my ($safariBundle) = @_;

    return "$safariBundle/Contents/MacOS/Safari" if isOSX();
    return $safariBundle if isCygwin();
}

sub installedSafariPath
{
    my $safariBundle;

    if (isOSX()) {
        $safariBundle = "/Applications/Safari.app";
    } elsif (isCygwin()) {
        my $findSafariName = "FindSafari";
        $findSafariName .= "_debug" if $configuration ne "Release";
        $findSafariName .= ".exe";

        $safariBundle = `"$configurationProductDir/$findSafariName"`;
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
        if (isOSX() && -d "$configurationProductDir/Safari.app") {
            $safariBundle = "$configurationProductDir/Safari.app";
        } elsif (isCygwin() && -x "$configurationProductDir/bin/Safari.exe") {
            $safariBundle = "$configurationProductDir/bin/Safari.exe";
        } else {
            return installedSafariPath();
        }
    }
    my $safariPath = safariPathFromSafariBundle($safariBundle);
    die "Can't find executable at $safariPath.\n" if isOSX() && !-x $safariPath;
    return $safariPath;
}

sub builtDylibPathForName
{
    my $framework = shift;
    determineConfigurationProductDir();
    if (isQt() or isGtk()) {
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
    return if isCygwin();
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

    if (isQt()) {
        return 1;
    }

    if (isGtk() and $path =~ /WebCore/) {
        $path .= "/../lib/libWebKitGtk.so" if !$ENV{WEBKITAUTOTOOLS};
        $path .= "/../.libs/libWebKitGtk.so" if $ENV{WEBKITAUTOTOOLS};
    }

    my $hasSVGSupport = 0;
    if (-e $path) {
        open NM, "-|", "nm", $path or die;
        while (<NM>) {
            $hasSVGSupport = 1 if /SVGElement/;
        }
        close NM;
    }
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
    determineIsQt();
    return $isQt;
}

sub checkArgv($)
{
    my $argToCheck = shift;
    foreach my $opt (@ARGV) {
        if ($opt =~ /^$argToCheck/i ) {
            @ARGV = grep(!/^$argToCheck/i, @ARGV);
            return 1;
        }
    }
    return 0;
}

sub determineIsQt()
{
    return if defined($isQt);

    # Allow override in case QTDIR is not set.
    if (checkArgv("--qt")) {
        $isQt = 1;
        return;
    }

    # The presence of QTDIR only means Qt if --gtk is not on the command-line
    if (isGtk()) {
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

    if (checkArgv("--gtk")) {
        $isGtk = 1;
    } else {
        $isGtk = 0;
    }
}

sub isWx()
{
    determineIsWx();
    return $isWx;
}

sub determineIsWx()
{
    return if defined($isWx);

    if (checkArgv("--wx")) {
        $isWx = 1;
    } else {
        $isWx = 0;
    }
}

# Determine if this is debian, ubuntu, linspire, or something similar.
sub isDebianBased()
{
    return -e "/etc/debian_version";
}

sub isCygwin()
{
    return ($^O eq "cygwin");
}

sub isDarwin()
{
    return ($^O eq "darwin");
}

sub isOSX()
{
    return isDarwin() unless (isQt() or isGtk() or isWx());
    return 0;
}

sub determineOSXVersion()
{
    return if $osXVersion;

    if (!isOSX()) {
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
    return isOSX() && osXVersion()->{"minor"} == 4;
}

sub isLeopard()
{
    return isOSX() && osXVersion()->{"minor"} == 5;
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
    if (isGtk() || isQt()) {
        return "$relativeScriptsPath/run-launcher";
    } elsif (isOSX() || isCygwin()) {
        return "$relativeScriptsPath/run-safari";
    }
}

sub launcherName()
{
    if (isGtk()) {
        return "GtkLauncher";
    } elsif (isQt()) {
        return "QtLauncher";
    } elsif (isOSX() || isCygwin()) {
        return "Safari";
    }
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
    } elsif (isGtk() or isQt() or isWx()) {
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

sub buildVisualStudioProject
{
    my ($project, $clean) = @_;
    setupCygwinEnv();

    my $config = configurationForVisualStudio();

    chomp(my $winProjectPath = `cygpath -w "$project"`);
    
    my $command = "/build";
    if ($clean) {
        $command = "/clean";
    }

    print "$vcBuildPath $winProjectPath /build $config\n";
    return system $vcBuildPath, $winProjectPath, $command, $config;
}

sub qtMakeCommand($)
{
    my ($qmakebin) = @_;
    chomp(my $mkspec = `$qmakebin -query QMAKE_MKSPECS`);
    $mkspec .= "/default";

    my $compiler = "";
    open SPEC, "<$mkspec/qmake.conf" or return "make";
    while (<SPEC>) {
        if ($_ =~ /QMAKE_CC\s*=\s*([^\s]+)/) {
            $compiler = $1;
        }
    }
    close SPEC;

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
    my ($clean, @buildArgs) = @_;

    my $make = 'make';
    my $dir = productDir();
    my $config = passedConfiguration() || configuration();
    my $prefix = $ENV{"WebKitInstallationPrefix"};

    # check if configuration is Debug
    if ($config =~ m/debug/i) {
        push @buildArgs, "--enable-debug";
    } else {
        push @buildArgs, "--disable-debug";
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
        $result = system $make, "distclean";
        return $result;
    }

    print "Calling configure in " . $dir . "\n\n";
    print "Installation directory: $prefix\n" if(defined($prefix));
     
    $result = system "$sourceDir/autogen.sh", @buildArgs;
    if ($result ne 0) {
        die "Failed to setup build environment using 'autotools'!\n";
    }

    $result = system $make;
    if ($result ne 0) {
        die "\nFailed to build WebKit using '$make'!\n";
    }

    chdir ".." or die;
    return $result;
}

sub buildQMakeProject($@)
{
    my ($clean, @buildArgs) = @_;

    push @buildArgs, "-r";

    my $qmakebin = "qmake"; # Allow override of the qmake binary from $PATH
    for my $i (0 .. $#ARGV) {
        my $opt = $ARGV[$i];
        if ($opt =~ /^--qmake=(.*)/i ) {
            $qmakebin = $1;
        } elsif ($opt =~ /^--qmakearg=(.*)/i ) {
            push @buildArgs, $1;
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
        push @buildArgs, "CONFIG+=release";
        push @buildArgs, "CONFIG-=debug";
    }

    my $dir = baseProductDir();
    if (! -d $dir) {
        system "mkdir", "-p", "$dir";
        if (! -d $dir) {
            die "Failed to create product directory " . $dir;
        }
    }
    $dir = $dir . "/$config";
    if (! -d $dir) {
        system "mkdir", "-p", "$dir";
        if (! -d $dir) {
            die "Failed to create build directory " . $dir;
        }
    }

    chdir $dir or die "Failed to cd into " . $dir . "\n";

    print "Calling '$qmakebin @buildArgs' in " . $dir . "\n\n";
    print "Installation directory: $prefix\n" if(defined($prefix));

    my $result = system $qmakebin, @buildArgs;
    if ($result ne 0) {
       die "Failed to setup build environment using $qmakebin!\n";
    }

    if ($clean) {
      $result = system "$make distclean";
    } else {
      $result = system "$make";
    }

    chdir ".." or die;
    return $result;
}

sub buildQMakeQtProject($$)
{
    my ($project, $clean) = @_;

    my @buildArgs = ("CONFIG+=qt-port");
    return buildQMakeProject($clean, @buildArgs);
}

sub buildGtkProject($$@)
{
    my ($project, $clean, @buildArgs) = @_;

    if ($project ne "WebKit") {
        die "The Gtk port builds JavaScriptCore, WebCore and WebKit in one shot! Only call it for 'WebKit'.\n";
    }

    if ($ENV{WEBKITAUTOTOOLS}) {
        return buildAutotoolsProject($clean, @buildArgs);
    } else {
        my @buildArgs = ("CONFIG+=gtk-port", "CONFIG-=qt");
        return buildQMakeProject($clean, @buildArgs);
    }
}

sub setPathForRunningWebKitApp
{
    my ($env) = @_;

    return unless isCygwin();

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

    if (isOSX()) {
        return system "$FindBin::Bin/gdb-safari", @ARGV if $debugger;

        my $productDir = productDir();
        print "Starting Safari with DYLD_FRAMEWORK_PATH set to point to built WebKit in $productDir.\n";
        $ENV{DYLD_FRAMEWORK_PATH} = $productDir;
        $ENV{WEBKIT_UNSET_DYLD_FRAMEWORK_PATH} = "YES";
        return system safariPath(), @ARGV;
    }

    if (isCygwin()) {
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

sub runDrosera
{
    my ($debugger) = @_;

    if (isOSX()) {
        return system "$FindBin::Bin/gdb-drosera", @ARGV if $debugger;

        my $productDir = productDir();
        print "Starting Drosera with DYLD_FRAMEWORK_PATH set to point to built WebKit in $productDir.\n";
        $ENV{DYLD_FRAMEWORK_PATH} = $productDir;
        $ENV{WEBKIT_UNSET_DYLD_FRAMEWORK_PATH} = "YES";

        my $droseraPath = "$productDir/Drosera.app/Contents/MacOS/Drosera";
        return system $droseraPath, @ARGV;
    }

    if (isCygwin()) {
        print "Running Drosera\n";
        my $script = "run-drosera-nightly.cmd";
        my $prodDir = productDir();
        my $result = system "cp", "$FindBin::Bin/$script", $prodDir;
        return $result if $result;

        my $cwd = getcwd();
        chdir $prodDir;

        my $debuggerFlag = $debugger ? "/debugger" : "";
        $result = system "cmd", "/c", "call $script $debuggerFlag";
        chdir $cwd;
        return $result;
    }

    return 1;
}

1;
