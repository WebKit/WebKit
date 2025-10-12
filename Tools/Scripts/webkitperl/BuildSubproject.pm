#!/usr/bin/env perl

# Copyright (C) 2005-2022 Apple Inc.  All rights reserved.
# Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

use strict;
use warnings;
use File::Basename;
use FindBin;
use Getopt::Long qw(:config pass_through);
use lib $FindBin::Bin;
use webkitdirs;
use webkitperl::FeatureList qw(getFeatureOptionList);
use POSIX;
use Text::ParseWords;

sub writeCongrats;

prohibitUnknownPort();

if (shouldUseFlatpak()) {
    print "Building flatpak based environment\n";
    my @command = (File::Spec->catfile(sourceDir(), "Tools", "Scripts", "build-jsc"));
    runInFlatpak(@command);
}


my $shouldRunStaticAnalyzer = 0;
my $minimal = 0;
my $coverageSupport = 0;
my $showHelp = 0;
my $ftlJIT = int(isAppleCocoaWebKit() && !willUseIOSSimulatorSDK() || ((isARM64() || isX86_64()) && (isGtk() || isJSCOnly())));
my $forceCLoop = 0;
my $rosetta = 0;
my $makeArgs = "";
my @cmakeArgs;
my $buildDir = "";
my $startTime = time();
my $useCCache = -1;
my $exportCompileCommands = 0;

my @features = getFeatureOptionList();

# Initialize values from defaults
foreach (@ARGV) {
    if ($_ eq '--minimal') {
        $minimal = 1;
    }
}

# Initialize values from defaults
foreach (@features) {
    ${$_->{value}} = ($minimal ? 0 : $_->{default});
}

# Additional environment parameters
push @ARGV, parse_line('\s+', 0, $ENV{'BUILD_JSC_ARGS'}) if ($ENV{'BUILD_JSC_ARGS'});

my $programName = basename($0);
my $usage = <<EOF;
Usage: $programName [options] [options to pass to build system]
  --help                        Show this help message
  --[no-]analyze                Toggle code static analysis (default: $shouldRunStaticAnalyzer)
  --[no-]coverage               Toggle code coverage support (default: $coverageSupport)
  --[no-]ftl-jit                Toggle FTL JIT support (default: $ftlJIT)
  --cloop                       Use C Loop interpreter (default: $forceCLoop)
  --rosetta                     Build JSC for testing under rosetta (default: $rosetta)
  --makeargs=<arguments>        Optional Makefile/xcodebuild flags
  --cmakeargs=<arguments>       One or more optional CMake flags (e.g. --cmakeargs="-DFOO=bar -DCMAKE_PREFIX_PATH=/usr/local")
  --build-dir=<path>            Build out of tree in directory at <path>

  --[no-]use-ccache             Enable (or disable) CCache, if available
  --export-compile-commands     Generate compile_commands.json (Apple platforms only)
EOF

my %options = (
    'analyze!' => \$shouldRunStaticAnalyzer,
    'coverage!' => \$coverageSupport,
    'help' => \$showHelp,
    'ftl-jit!' => \$ftlJIT,
    'cloop!' => \$forceCLoop,
    'rosetta!' => \$rosetta,
    'makeargs=s' => \$makeArgs,
    'cmakeargs=s' => \@cmakeArgs,
    'build-dir=s' => \$buildDir,
    'use-ccache!' => \$useCCache,
    'export-compile-commands' => \$exportCompileCommands
);

foreach (@features) {
    if ($_->{javascript}) {
        my $opt = sprintf("%-35s", "  --[no-]$_->{option}");
        $usage .= "$opt $_->{desc} (default: $_->{default})\n";
        $options{"$_->{option}!"} = $_->{value};
    }
}

GetOptions(%options);

if ($showHelp) {
   print STDERR $usage;
   exit 1;
}

if (isAnyWindows()) {
    $ftlJIT = 0;
}

if ($buildDir && !isCMakeBuild()) {
    print STDERR "--build-dir is currently only supported for cmake builds";
    exit 1;
}

if ($useCCache == 1) {
    $ENV{'WK_USE_CCACHE'} = "YES";
} elsif ($useCCache == 0) {
    $ENV{'WK_USE_CCACHE'} = "NO";
}

$ENV{'EXPORT_COMPILE_COMMANDS'} = "YES" if $exportCompileCommands;

checkRequiredSystemConfig();
setConfiguration();
shouldBuild32Bit();  # Consume the --32-bit flag if passed and set internal state.
chdirWebKit();

if ($forceCLoop) {
    $ftlJIT = 0;
}

if (isCMakeBuild()) {
    if (scalar(@ARGV) > 0) {
        foreach (@ARGV) {
            my $arg = $_;
            if ($arg =~ /^-.*/) {
                print STDERR "Unrecognized option `$arg'\n";
            } else {
                print STDERR "Stray anonymous argument `$arg'\n";
            }
        }
        exit 2;
    }
    if ($forceCLoop) {
        push @cmakeArgs, " -DENABLE_JIT=OFF";
        push @cmakeArgs, " -DENABLE_C_LOOP=ON";
    }
    push @cmakeArgs, $ftlJIT ? " -DENABLE_FTL_JIT=ON" : " -DENABLE_FTL_JIT=OFF";
    if ($buildDir) {
        push @cmakeArgs, " " . sourceDir();
        setBaseProductDir($buildDir);
    }

    my @featureArgs = cmakeArgsFromFeatures(@features);

    my $buildTarget = "";
    unless (isAnyWindows()) {
        # By default we build using all of the available CPUs
        $makeArgs .= ($makeArgs ? " " : "") . "-j" . numberOfCPUs() if $makeArgs !~ /-j\s*\d+/;
        $buildTarget = "jsc testb3 testair testapi testmasm testdfg testwasmdebugger $makeArgs";
    } elsif (canUseNinja()) {
        $buildTarget .= "jsc testapi testmasm";
    }

    # This call only returns if nothing wrong happened
    buildCMakeProjectOrExit(0, undef, $buildTarget, @featureArgs, @cmakeArgs);
    writeCongrats("JavaScriptCore");
    exit exitStatus(0);
}

if (isAppleCocoaWebKit()) {
    my @options = XcodeOptions();
    # FIXME: additionalSupportOptions is unused. Coverage and static analysis options are broken.
    # https://webkit.org/b/259562
    my @additionalSupportOptions = ();
    push @additionalSupportOptions, XcodeCoverageSupportOptions() if $coverageSupport;
    push @additionalSupportOptions, XcodeStaticAnalyzerOption() if $shouldRunStaticAnalyzer;
    push @additionalSupportOptions, XcodeExportCompileCommandsOptions() if $exportCompileCommands;

    push @options, ($forceCLoop ? "ENABLE_JIT=ENABLE_JIT=0" : "ENABLE_JIT=ENABLE_JIT");
    push @options, ($forceCLoop ? "ENABLE_C_LOOP=ENABLE_C_LOOP" : "ENABLE_C_LOOP=ENABLE_C_LOOP=0");
    push @options, ($ftlJIT ? "ENABLE_FTL_JIT=ENABLE_FTL_JIT" : "ENABLE_FTL_JIT=ENABLE_FTL_JIT=0");

    sub option($$$)
    {
        my ($feature, $isEnabled, $defaultValue) = @_;
        return "" if $defaultValue == $isEnabled;
        return $feature . "=" . ($isEnabled ? $feature : "");
    }

    foreach (@features) {
        if ($_->{javascript}) {
            my $option = option($_->{define}, ${$_->{value}}, $_->{default});
            push @options, $option unless $option eq "";
        }
    }
}

sub buildUpToProject
{
    my ($projectDirectory, $projectName) = @_;
    my $result;

    chdir $projectDirectory or die "Can't find $projectName directory to build from";
    if (isAppleCocoaWebKit()) {
        configuredXcodeWorkspace() or die "Can't determine configured Xcode workspace";

        # By convention, projects that support this build workflow
        # (JavaScriptCore, WebGPU) have a scheme which builds that project
        # and its implicit dependencies.
        my $schemeName = "Everything up to $projectName";
        my @extraCommands;
        my @compilerFlags;

        push @compilerFlags, $makeArgs;
        if ($forceCLoop) {
            push @compilerFlags, "ENABLE_JIT=0 ENABLE_C_LOOP=1";
        }

        if ($rosetta) {
            push @compilerFlags, "HAVE_CPU_TRANSLATION_CAPABILITY=0";
            push @extraCommands, "SKIP_ROSETTA_BREAKING_ENTITLEMENTS=1 ARCHS=x86_64";
        }

        foreach (@ARGV) {
            # Ensure that if somebody passes in a flag like `ARCHS="i386 x86_64"` we
            # maintain the quotes around "i386 x86_64".
            my $arg = $_;
            if ($arg =~ m/=/) {
                $arg =~ s/=/="/;
                $arg = $arg . "\"";
            }
            push @extraCommands, $arg;
        }

        my $command = "make SCHEME=\"$schemeName\" " . (lc configuration()) . " GCC_PREPROCESSOR_ADDITIONS=\"@compilerFlags\" @extraCommands";

        print "\n";
        print "building ", $projectName, "\n";
        print "running build command '", $command, "' in ", Cwd::cwd(), "\n\n";

        $result = system $command;
    } else {
        die "Building not defined for this platform!\n";
    }
    exit exitStatus($result) if exitStatus($result);
    chdirWebKit();
}

sub writeCongrats
{
    my ($projectName) = @_;
    my $endTime = time();
    my $buildTime = formatBuildTime($endTime - $startTime);

    print "\n";
    print "====================================================================\n";
    print " " . $projectName . " is now built ($buildTime). \n";
    print "====================================================================\n";
}

1;
