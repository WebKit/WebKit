#!/usr/bin/env perl

# Copyright (C) 2015-2018 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

use warnings;
use English;
use File::Basename qw(dirname);
use File::Copy qw(copy);
use File::Path qw(make_path remove_tree);
use File::Spec;
use Getopt::Long;

my $verbose = 0;
GetOptions('verbose' => \$verbose);

sub debugLog($)
{
    my $logString = shift;
    print "-- $logString\n" if $verbose;
}

my $useDirCopy = 0;

# Not all systems (e.g., OS X) include File::Copy::Recursive. Only
# use it if we have it installed.
eval "use File::Copy::Recursive";
unless ($@) {
    $useDirCopy = 1;
    File::Copy::Recursive->import();
}

sub ditto($$)
{
    my ($source, $destination) = @_;

    if ($useDirCopy) {
        File::Copy::Recursive::dircopy($source, $destination) or die "Unable to copy directory $source to $destination: $!";
    } elsif ($^O eq 'darwin') {
        system('ditto', $source, $destination);
    } elsif ($^O ne 'MSWin32') {
        # Ditto copies the *contents* of the source directory, not the directory itself.
        opendir(my $dh, $source) or die "Can't open $source: $!";
        make_path($destination);
        while (readdir $dh) {
            if ($_ ne '..' and $_ ne '.') {
                system('cp', '-R', "${source}/$_", $destination) == 0 or die "Failed to copy ${source}/$_ to $destination";
            }
        }
        closedir $dh;
    } else {
        die "Please install the PEP module File::Copy::Recursive";
    }
}

sub seedFile($$)
{
    my ($targetFile, $seedText) = @_;

    if (open(TARGET_FILE, '>', $targetFile)) {
        print TARGET_FILE $seedText;
        close(TARGET_FILE);
    }
}

sub appendFile($$)
{
    my ($targetFile, $srcFile) = @_;

    open(SRC_FILE, '<', $srcFile) or die "Unable to open $srcFile: $!";
    my @srcText = <SRC_FILE>;
    close(SRC_FILE);
    open(TARGET_FILE, '>>', $targetFile) or die "Unable to open $targetFile: $!";
    print TARGET_FILE @srcText;
    close(TARGET_FILE);
}

sub readLicenseFile($)
{
    my ($licenseFilePath) = @_;

    open(LICENSE_FILE, '<', $licenseFilePath) or die "Unable to open $licenseFilePath: $!";

    my $license = "/*\n";
    $license .= ' * ' . $_ while <LICENSE_FILE>;
    $license .= " */\n";

    close(LICENSE_FILE);

    return $license;
}

my $inspectorLicense = <<'EOF';
/*
 * Copyright (C) 2007-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek. All rights reserved.
 * Copyright (C) 2008-2009 Anthony Ricaud <rik@webkit.org>
 * Copyright (C) 2009-2010 Joseph Pecoraro. All rights reserved.
 * Copyright (C) 2009-2011 Google Inc. All rights reserved.
 * Copyright (C) 2009 280 North Inc. All Rights Reserved.
 * Copyright (C) 2010 Nikita Vasilyev. All rights reserved.
 * Copyright (C) 2011 Brian Grinstead All rights reserved.
 * Copyright (C) 2013 Matt Holden <jftholden@yahoo.com>
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 * Copyright (C) 2013 Seokju Kwon (seokju.kwon@gmail.com)
 * Copyright (C) 2013 Adobe Systems Inc. All rights reserved.
 * Copyright (C) 2013-2015 University of Washington. All rights reserved.
 * Copyright (C) 2014-2015 Saam Barati <saambarati1@gmail.com>
 * Copyright (C) 2014 Antoine Quint
 * Copyright (C) 2015 Tobias Reiss <tobi+webkit@basecode.de>
 * Copyright (C) 2015-2017 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
 * Copyright (C) 2017 The Chromium Authors
 * Copyright (C) 2017-2018 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
EOF

my $perl = $^X;
my $python = ($OSNAME =~ /cygwin/) ? "/usr/bin/python" : "python";
my $derivedSourcesDir = $ENV{'DERIVED_SOURCES_DIR'};
my $scriptsRoot = File::Spec->catdir($ENV{'SRCROOT'}, 'Scripts');
my $sharedScriptsRoot = File::Spec->catdir($ENV{'JAVASCRIPTCORE_PRIVATE_HEADERS_DIR'});
my $uiRoot = File::Spec->catdir($ENV{'SRCROOT'}, 'UserInterface');
my $targetResourcePath = File::Spec->catdir($ENV{'TARGET_BUILD_DIR'}, $ENV{'UNLOCALIZED_RESOURCES_FOLDER_PATH'});
my $protocolDir = File::Spec->catdir($targetResourcePath, 'Protocol');
my $workersDir = File::Spec->catdir($targetResourcePath, 'Workers');
my $codeMirrorPath = File::Spec->catdir($uiRoot, 'External', 'CodeMirror');
my $esprimaPath = File::Spec->catdir($uiRoot, 'External', 'Esprima');
my $threejsPath = File::Spec->catdir($uiRoot, 'External', 'three.js');

$webInspectorUIAdditionsDir = &webInspectorUIAdditionsDir();

my $codeMirrorLicense = readLicenseFile(File::Spec->catfile($codeMirrorPath, 'LICENSE'));
my $esprimaLicense = readLicenseFile(File::Spec->catfile($esprimaPath, 'LICENSE'));
my $threejsLicense = readLicenseFile(File::Spec->catfile($threejsPath, 'LICENSE'));
make_path($protocolDir, $targetResourcePath);

$python = $ENV{"PYTHON"} if defined($ENV{"PYTHON"});

# Copy over dynamically loaded files from other frameworks, even if we aren't combining resources.
copy(File::Spec->catfile($ENV{'JAVASCRIPTCORE_PRIVATE_HEADERS_DIR'}, 'InspectorBackendCommands.js'), File::Spec->catfile($protocolDir, 'InspectorBackendCommands.js')) or die "Copy of InspectorBackendCommands.js failed: $!";

my $forceToolInstall = defined $ENV{'FORCE_TOOL_INSTALL'} && ($ENV{'FORCE_TOOL_INSTALL'} eq 'YES');
my $shouldCombineMain = defined $ENV{'COMBINE_INSPECTOR_RESOURCES'} && ($ENV{'COMBINE_INSPECTOR_RESOURCES'} eq 'YES');
my $shouldCombineTest = defined $ENV{'COMBINE_TEST_RESOURCES'} && ($ENV{'COMBINE_TEST_RESOURCES'} eq 'YES');
my $combineResourcesCmd = File::Spec->catfile($scriptsRoot, 'combine-resources.pl');

if ($forceToolInstall) {
    # Copy all files over individually to ensure we have Test.html / Test.js and files included from Test.html.
    # We may then proceed to include combined & optimized resources which will output mostly to different paths
    # but overwrite Main.html / Main.js with optimized versions.
    ditto($uiRoot, $targetResourcePath);

    # Also force combining test resources for tool installs.
    $shouldCombineTest = 1;
}

if (!$shouldCombineMain) {
    # Keep the files separate for engineering builds. Copy these before altering Main.html
    # in other ways, such as combining for WebKitAdditions or inlining files.
    ditto($uiRoot, $targetResourcePath);
}

# Always refer to the copy in derived sources so the order of replacements does not matter.
make_path($derivedSourcesDir);
my $derivedSourcesMainHTML = File::Spec->catfile($derivedSourcesDir, 'Main.html');
copy(File::Spec->catfile($uiRoot, 'Main.html'), File::Spec->catfile($derivedSourcesDir, 'Main.html')) or die "Copy failed: $!";

sub webInspectorUIAdditionsDir() {
    my $webkitAdditionsDir;
    if (defined $ENV{'BUILT_PRODUCTS_DIR'}) {
        $webkitAdditionsDir = File::Spec->catdir($ENV{'BUILT_PRODUCTS_DIR'}, 'usr', 'local', 'include', 'WebKitAdditions');
        undef $webkitAdditionsDir unless -d $webkitAdditionsDir
    }
    if (!$webkitAdditionsDir and defined $ENV{'SDKROOT'}) {
        $webkitAdditionsDir = File::Spec->catdir($ENV{'SDKROOT'}, 'usr', 'local', 'include', 'WebKitAdditions');
        undef $webkitAdditionsDir unless -d $webkitAdditionsDir
    }
    return unless $webkitAdditionsDir;
    debugLog("webkitAdditionsDir: $webkitAdditionsDir");
    return File::Spec->catdir($webkitAdditionsDir, 'WebInspectorUI');
}

sub combineOrStripResourcesForWebKitAdditions() {
    my $combineWebKitAdditions = 0;

    if (!defined $webInspectorUIAdditionsDir) {
        debugLog("Didn't define \$webInspectorUIAdditionsDir");
    } elsif (-d $webInspectorUIAdditionsDir) {
        debugLog("Found $webInspectorUIAdditionsDir");

        opendir(DIR, $webInspectorUIAdditionsDir) || die "$!";
        my @files = grep { !/^\.{1,2}$/ } readdir (DIR);
        closedir(DIR);

        # The WebKitAdditions/WebInspectorUI directory may exist without any files in it.
        # Make sure that there is something to be processed in the directory before proceeding.
        my $foundJSFile = 0;
        my $foundCSSFile = 0;
        foreach my $file (@files) {
            my $path = File::Spec->catdir($$webInspectorUIAdditionsDir, $file);
            next if -d $path;
            if ($file =~ /\.js$/) {
                debugLog("Found a JavaScript file to combine: $file");
                $foundJSFile = 1;
            } elsif ($file =~ /\.css$/) {
                debugLog("Found a CSS file to combine: $file");
                $foundCSSFile = 1;
            }
        }
        $combineWebKitAdditions = 1 if $foundCSSFile or $foundJSFile;
    } else {
        debugLog("Didn't find $webInspectorUIAdditionsDir");
    }

    if ($combineWebKitAdditions) {
        debugLog("Combining resources provided by WebKitAdditions.");
        combineResourcesForWebKitAdditions();
    } else {
        debugLog("Stripping resources provided by WebKitAdditions.");
        stripResourcesForWebKitAdditions();
    }
}

sub stripResourcesForWebKitAdditions() {
    system($perl, $combineResourcesCmd,
        '--input-dir', 'WebKitAdditions',
        '--input-html', $derivedSourcesMainHTML,
        '--derived-sources-dir', $derivedSourcesDir,
        '--output-dir', $derivedSourcesDir,
        '--strip');
}

sub combineResourcesForWebKitAdditions() {
    $rootPathForRelativeIncludes = dirname(dirname($webInspectorUIAdditionsDir));
    system($perl, $combineResourcesCmd,
        '--input-dir', 'WebKitAdditions',
        '--input-html', $derivedSourcesMainHTML,
        '--input-html-dir', $rootPathForRelativeIncludes,
        '--derived-sources-dir', $derivedSourcesDir,
        '--output-dir', $derivedSourcesDir,
        '--output-script-name', 'WebKitAdditions.js',
        '--output-style-name', 'WebKitAdditions.css');

    # Export the license into WebKitAdditions files.
    my $targetWebKitAdditionsJS = File::Spec->catfile($targetResourcePath, 'WebKitAdditions.js');
    seedFile($targetWebKitAdditionsJS, $inspectorLicense);

    my $targetWebKitAdditionsCSS = File::Spec->catfile($targetResourcePath, 'WebKitAdditions.css');
    seedFile($targetWebKitAdditionsCSS, $inspectorLicense);

    appendFile($targetWebKitAdditionsJS, File::Spec->catfile($derivedSourcesDir, 'WebKitAdditions.js'));
    appendFile($targetWebKitAdditionsCSS, File::Spec->catfile($derivedSourcesDir, 'WebKitAdditions.css'));
}

if ($shouldCombineMain) {
    # Remove Debug JavaScript and CSS files in Production builds.
    system($perl, $combineResourcesCmd,
        '--input-dir', 'Debug',
        '--input-html', $derivedSourcesMainHTML,
        '--input-html-dir', $uiRoot,
        '--derived-sources-dir', $derivedSourcesDir,
        '--output-dir', $derivedSourcesDir,
        '--output-script-name', 'Debug.js',
        '--output-style-name', 'Debug.css',
        '--strip');

    # Combine the JavaScript and CSS files in Production builds into single files (Main.js and Main.css).
    system($perl, $combineResourcesCmd,
       '--input-html', $derivedSourcesMainHTML,
       '--input-html-dir', $uiRoot,
       '--derived-sources-dir', $derivedSourcesDir,
       '--output-dir', $derivedSourcesDir,
       '--output-script-name', 'Main.js',
       '--output-style-name', 'Main.css');

    # Process WebKitAdditions.{css,js} after Main.{js,css}. Otherwise, the combined WebKitAdditions files
    # will get slurped into Main.{js,css} because the 'WebKitAdditions' relative URL prefix will be removed.
    combineOrStripResourcesForWebKitAdditions();

    # Combine the CodeMirror JavaScript and CSS files in Production builds into single files (CodeMirror.js and CodeMirror.css).
    system($perl, $combineResourcesCmd,
       '--input-dir', 'External/CodeMirror',
       '--input-html', $derivedSourcesMainHTML,
       '--input-html-dir', $uiRoot,
       '--derived-sources-dir', $derivedSourcesDir,
       '--output-dir', $derivedSourcesDir,
       '--output-script-name', 'CodeMirror.js',
       '--output-style-name', 'CodeMirror.css');

    # Combine the Esprima JavaScript files in Production builds into a single file (Esprima.js).
    system($perl, $combineResourcesCmd,
       '--input-dir', 'External/Esprima',
       '--input-html', $derivedSourcesMainHTML,
       '--input-html-dir', $uiRoot,
       '--derived-sources-dir', $derivedSourcesDir,
       '--output-dir', $derivedSourcesDir,
       '--output-script-name', 'Esprima.js');

    # Combine the three.js JavaScript files in Production builds into a single file (Three.js).
    system($perl, $combineResourcesCmd,
       '--input-dir', 'External/three.js',
       '--input-html', $derivedSourcesMainHTML,
       '--input-html-dir', $uiRoot,
       '--derived-sources-dir', $derivedSourcesDir,
       '--output-dir', $derivedSourcesDir,
       '--output-script-name', 'Three.js');

    # Remove console.assert calls from the Main.js file.
    my $derivedSourcesMainJS = File::Spec->catfile($derivedSourcesDir, 'Main.js');
    system($perl, File::Spec->catfile($scriptsRoot, 'remove-console-asserts.pl'),
        '--input-script', $derivedSourcesMainJS,
        '--output-script', $derivedSourcesMainJS);

    # Fix Image URLs in the Main.css file by removing the "../".
    my $derivedSourcesMainCSS = File::Spec->catfile($derivedSourcesDir, 'Main.css');
    if (open(INPUT_MAIN, '<', $derivedSourcesMainCSS)) {
        local $/;
        my $cssContents = <INPUT_MAIN>;
        close(INPUT_MAIN);
        open(OUTPUT_MAIN, '>', $derivedSourcesMainCSS);
        $cssContents =~ s/\.\.\/Images/Images/g;
        print OUTPUT_MAIN $cssContents;
        close(OUTPUT_MAIN);
    }

    # Export the license into Main.js.
    my $targetMainJS = File::Spec->catfile($targetResourcePath, 'Main.js');
    seedFile($targetMainJS, $inspectorLicense);

    my $targetMainCSS = File::Spec->catfile($targetResourcePath, 'Main.css');
    seedFile($targetMainCSS, $inspectorLicense);

    # Export the license into CodeMirror.js and CodeMirror.css.
    my $targetCodeMirrorJS = File::Spec->catfile($targetResourcePath, 'CodeMirror.js');
    seedFile($targetCodeMirrorJS, $codeMirrorLicense);

    my $targetCodeMirrorCSS = File::Spec->catfile($targetResourcePath, 'CodeMirror.css');
    seedFile($targetCodeMirrorCSS, $codeMirrorLicense);

    # Export the license into Esprima.js.
    my $targetEsprimaJS = File::Spec->catfile($targetResourcePath, 'Esprima.js');
    seedFile($targetEsprimaJS, $esprimaLicense);

    # Export the license into Three.js.
    my $targetThreejsJS = File::Spec->catfile($targetResourcePath, 'Three.js');
    seedFile($targetThreejsJS, $threejsLicense);

    # Minify the Main.js and Main.css files, with Main.js appending to the license that was exported above.
    my $jsMinScript = File::Spec->catfile($sharedScriptsRoot, 'jsmin.py');
    my $cssMinScript = File::Spec->catfile($sharedScriptsRoot, 'cssmin.py');
    system(qq("$python" "$jsMinScript" < "$derivedSourcesMainJS" >> "$targetMainJS")) and die "Failed to minify $derivedSourcesMainJS: $!";
    system(qq("$python" "$cssMinScript" < "$derivedSourcesMainCSS" >> "$targetMainCSS")) and die "Failed to minify $derivedSourcesMainCSS: $!";

    # Minify the CodeMirror.js and CodeMirror.css files, appending to the license that was exported above.
    my $derivedSourcesCodeMirrorJS = File::Spec->catfile($derivedSourcesDir, 'CodeMirror.js');
    my $derivedSourcesCodeMirrorCSS = File::Spec->catfile($derivedSourcesDir, 'CodeMirror.css');
    system(qq("$python" "$jsMinScript" < "$derivedSourcesCodeMirrorJS" >> "$targetCodeMirrorJS")) and die "Failed to minify $derivedSourcesCodeMirrorJS: $!";
    system(qq("$python" "$cssMinScript" < "$derivedSourcesCodeMirrorCSS" >> "$targetCodeMirrorCSS")) and die "Failed to minify $derivedSourcesCodeMirrorCSS: $!";

    # Minify the Esprima.js file, appending to the license that was exported above.
    my $derivedSourcesEsprimaJS = File::Spec->catfile($derivedSourcesDir, 'Esprima.js');
    system(qq("$python" "$jsMinScript" < "$derivedSourcesEsprimaJS" >> "$targetEsprimaJS")) and die "Failed to minify $derivedSourcesEsprimaJS: $!";

    # Minify the Three.js file, appending to the license that was exported above.
    my $derivedSourcesThreejsJS = File::Spec->catfile($derivedSourcesDir, 'Three.js');
    system(qq("$python" "$jsMinScript" < "$derivedSourcesThreejsJS" >> "$targetThreejsJS")) and die "Failed to minify $derivedSourcesThreejsJS: $!";

    # Copy over the Images directory.
    ditto(File::Spec->catdir($uiRoot, 'Images'), File::Spec->catdir($targetResourcePath, 'Images'));

    # Copy the Protocol/Legacy and Workers directories.
    ditto(File::Spec->catfile($uiRoot, 'Protocol', 'Legacy'), File::Spec->catfile($protocolDir, 'Legacy'));
    ditto(File::Spec->catfile($uiRoot, 'Workers'), $workersDir);

    # Remove console.assert calls from the Worker js files.
    system($perl, File::Spec->catfile($scriptsRoot, 'remove-console-asserts.pl'),
        '--input-directory', $workersDir);

    # Fix import references in Workers directories. This rewrites "../../External/script.js" import paths to their new locations.
    system($perl, File::Spec->catfile($scriptsRoot, 'fix-worker-imports-for-optimized-builds.pl'),
        '--input-directory', $workersDir) and die "Failed to update Worker imports for optimized builds.";
} else {
    # Always process WebKitAdditions files because the 'WebKitAdditions' path prefix is not real,
    # so it can't proceed as a normal load from the bundle as written. This function replaces the
    # dummy prefix with the actual WebKitAdditions path when looking for files to inline and combine.
    combineOrStripResourcesForWebKitAdditions();
}

# Always copy over Main.html because we may have combined WebKitAdditions files
# without minifying anything else. We always want to combine WKA so the relevant
# resources are copied out of Derived Sources rather than an arbitrary WKA directory.
copy($derivedSourcesMainHTML, File::Spec->catfile($targetResourcePath, 'Main.html'));

if ($shouldCombineTest) {
    # Combine the JavaScript files for testing into a single file (TestCombined.js).
    system($perl, $combineResourcesCmd,
        '--input-html', File::Spec->catfile($uiRoot, 'Test.html'),
        '--derived-sources-dir', $derivedSourcesDir,
        '--output-dir', $derivedSourcesDir,
        '--output-script-name', 'TestCombined.js',
        '--output-style-name', 'TestCombined.css');

    my $derivedSourcesTestHTML = File::Spec->catfile($derivedSourcesDir, 'Test.html');
    my $derivedSourcesTestJS = File::Spec->catfile($derivedSourcesDir, 'TestCombined.js');
    # Combine the CodeMirror JavaScript files into single file (TestCodeMirror.js).
    system($perl, $combineResourcesCmd,
        '--input-dir', 'External/CodeMirror',
        '--input-html', $derivedSourcesTestHTML,
        '--input-html-dir', $uiRoot,
        '--derived-sources-dir', $derivedSourcesDir,
        '--output-dir', $derivedSourcesDir,
        '--output-script-name', 'TestCodeMirror.js');

    # Combine the Esprima JavaScript files for testing into a single file (TestEsprima.js).
    system($perl, $combineResourcesCmd,
        '--input-dir', 'External/Esprima',
        '--input-html', $derivedSourcesTestHTML,
        '--input-html-dir', $uiRoot,
        '--derived-sources-dir', $derivedSourcesDir,
        '--output-dir', $derivedSourcesDir,
        '--output-script-name', 'TestEsprima.js');

    # Export the license into TestCombined.js.
    my $targetTestJS = File::Spec->catfile($targetResourcePath, 'TestCombined.js');
    seedFile($targetTestJS, $inspectorLicense);

    # Export the license into TestCodeMirror.js.
    my $targetCodeMirrorJS = File::Spec->catfile($targetResourcePath, 'TestCodeMirror.js');
    seedFile($targetCodeMirrorJS, $codeMirrorLicense);

    # Export the license into TestEsprima.js.
    my $targetEsprimaJS = File::Spec->catfile($targetResourcePath, 'TestEsprima.js');
    seedFile($targetEsprimaJS, $esprimaLicense);

    # Append TestCombined.js to the license that was exported above.
    appendFile($targetTestJS, $derivedSourcesTestJS);

    # Append CodeMirror.js to the license that was exported above.
    my $derivedSourcesCodeMirrorJS = File::Spec->catfile($derivedSourcesDir, 'TestCodeMirror.js');
    appendFile($targetCodeMirrorJS, $derivedSourcesCodeMirrorJS);

    # Append Esprima.js to the license that was exported above.
    my $derivedSourcesEsprimaJS = File::Spec->catfile($derivedSourcesDir, 'TestEsprima.js');
    appendFile($targetEsprimaJS, $derivedSourcesEsprimaJS);

    # Copy over Test.html.
    copy($derivedSourcesTestHTML, File::Spec->catfile($targetResourcePath, 'Test.html'));

    # Combine the JavaScript files for testing into a single file (TestStub.js).
    system($perl, $combineResourcesCmd,
        '--input-html', File::Spec->catfile($uiRoot, 'TestStub.html'),
        '--derived-sources-dir', $derivedSourcesDir,
        '--output-dir', $derivedSourcesDir,
        '--output-script-name', 'TestStubCombined.js');

    # Copy over TestStub.html and TestStubCombined.js.
    copy(File::Spec->catfile($derivedSourcesDir, 'TestStub.html'), File::Spec->catfile($targetResourcePath, 'TestStub.html'));
    copy(File::Spec->catfile($derivedSourcesDir, 'TestStubCombined.js'), File::Spec->catfile($targetResourcePath, 'TestStubCombined.js'));

    # Copy the Legacy directory.
    ditto(File::Spec->catfile($uiRoot, 'Protocol', 'Legacy'), File::Spec->catfile($protocolDir, 'Legacy'));
}
