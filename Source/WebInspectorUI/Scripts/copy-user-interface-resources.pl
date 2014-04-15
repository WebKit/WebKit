#!/usr/bin/perl -w

use File::Copy qw(copy);
use File::Path qw(make_path);
use File::Spec;

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

my $LICENSE = <<'EOF';
/*
 * Copyright (C) 2007-2014 Apple Inc. All rights reserved.
 * Copyright (C) 2009-2011 Google Inc. All rights reserved.
 * Copyright (C) 2009-2010 Joseph Pecoraro. All rights reserved.
 * Copyright (C) 2008 Matt Lilek. All rights reserved.
 * Copyright (C) 2008-2009 Anthony Ricaud <rik@webkit.org>
 * Copyright (C) 2009 280 North Inc. All Rights Reserved.
 * Copyright (C) 2010 Nikita Vasilyev. All rights reserved.
 * Copyright (C) 2011 Brian Grinstead All rights reserved.
 * Copyright (C) 2013 Matt Holden <jftholden@yahoo.com>
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 * Copyright (C) 2013 Seokju Kwon (seokju.kwon@gmail.com)
 * Copyright (C) 2013 Adobe Systems Inc. All rights reserved.
 * Copyright (C) 2013 University of Washington. All rights reserved.
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

my $derivedSourcesDir = $ENV{'DERIVED_SOURCES_DIR'};
my $scriptsRoot = File::Spec->catdir($ENV{'SRCROOT'}, 'Scripts');
my $uiRoot = File::Spec->catdir($ENV{'SRCROOT'}, 'UserInterface');
my $targetResourcePath = File::Spec->catdir($ENV{'TARGET_BUILD_DIR'}, $ENV{'UNLOCALIZED_RESOURCES_FOLDER_PATH'});
my $protocolDir = File::Spec->catdir($targetResourcePath, 'Protocol');
my $codeMirrorPath = File::Spec->catdir($uiRoot, 'External', 'CodeMirror');

my $codeMirrorLicenseFile = File::Spec->catfile($codeMirrorPath, 'LICENSE');
open(CMLFILE, '<', $codeMirrorLicenseFile) or die "Unable to open $codeMirrorLicenseFile: $!";
my $CODE_MIRROR_LICENSE = "/*\n";
while (<CMLFILE>) {
    $CODE_MIRROR_LICENSE .= ' * ' . $_;
}
close(CMLFILE);
$CODE_MIRROR_LICENSE .= " */\n";

make_path($protocolDir, $targetResourcePath);

# Copy over dynamically loaded files from other frameworks, even if we aren't combining resources.
copy(File::Spec->catfile($ENV{'JAVASCRIPTCORE_PRIVATE_HEADERS_DIR'}, 'InspectorJSBackendCommands.js'), File::Spec->catfile($protocolDir, 'InspectorJSBackendCommands.js')) or die "Copy of InspectorJSBackendCommands.js failed: $!";
copy(File::Spec->catfile($ENV{'WEBCORE_PRIVATE_HEADERS_DIR'}, 'InspectorWebBackendCommands.js'), File::Spec->catfile($protocolDir, 'InspectorWebBackendCommands.js')) or die "Copy of InspectorWebBackendCommands.js failed: $!";

if (defined $ENV{'COMBINE_INSPECTOR_RESOURCES'} && ($ENV{'COMBINE_INSPECTOR_RESOURCES'} eq 'YES')) {
    my $combineResourcesCmd = File::Spec->catfile($scriptsRoot, 'combine-resources.pl');

    # Combine the JavaScript and CSS files in Production builds into single files (Main.js and Main.css).
    system($combineResourcesCmd, '--input-html', File::Spec->catfile($uiRoot, 'Main.html'), '--derived-sources-dir', $derivedSourcesDir, '--output-dir', $derivedSourcesDir, '--output-script-name', 'Main.js', '--output-style-name', 'Main.css');

    # Combine the CodeMirror JavaScript and CSS files in Production builds into single files (CodeMirror.js and CodeMirror.css).
    my $derivedSourcesMainHTML = File::Spec->catfile($derivedSourcesDir, 'Main.html');
    system($combineResourcesCmd, '--input-dir', 'External/CodeMirror', '--input-html', $derivedSourcesMainHTML, '--input-html-dir', $uiRoot, '--derived-sources-dir', $derivedSourcesDir, '--output-dir', $derivedSourcesDir, '--output-script-name', 'CodeMirror.js', '--output-style-name', 'CodeMirror.css');

    # Remove console.assert calls from the Main.js file.
    my $derivedSourcesMainJS = File::Spec->catfile($derivedSourcesDir, 'Main.js');
    system(File::Spec->catfile($scriptsRoot, 'remove-console-asserts.pl'), '--input-script', $derivedSourcesMainJS, '--output-script', $derivedSourcesMainJS);

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
    seedFile($targetMainJS, $LICENSE);

    my $targetMainCSS = File::Spec->catfile($targetResourcePath, 'Main.css');
    seedFile($targetMainCSS, $LICENSE);

    # Export the license into CodeMirror.js and CodeMirror.css.
    my $targetCodeMirrorJS = File::Spec->catfile($targetResourcePath, 'CodeMirror.js');
    seedFile($targetCodeMirrorJS, $CODE_MIRROR_LICENSE);

    my $targetCodeMirrorCSS = File::Spec->catfile($targetResourcePath, 'CodeMirror.css');
    seedFile($targetCodeMirrorCSS, $CODE_MIRROR_LICENSE);

    # Minify the Main.js and Main.css files, with Main.js appending to the license that was exported above.
    my $jsMinScript = File::Spec->catfile($scriptsRoot, 'jsmin.py');
    my $cssMinScript = File::Spec->catfile($scriptsRoot, 'cssmin.py');
    system(qq(python "$jsMinScript" < "$derivedSourcesMainJS" >> "$targetMainJS")) and die "Failed to minify $derivedSourcesMainJS: $!";
    system(qq(python "$cssMinScript" < "$derivedSourcesMainCSS" >> "$targetMainCSS")) and die "Failed to minify $derivedSourcesMainCSS: $!";

    # Minify the CodeMirror.js and CodeMirror.css files, appending to the license that was exported above.
    my $derivedSouressCodeMirrorJS = File::Spec->catfile($derivedSourcesDir, 'CodeMirror.js');
    my $derivedSourcesCodeMirrorCSS = File::Spec->catfile($derivedSourcesDir, 'CodeMirror.css');
    system(qq(python "$jsMinScript" < "$derivedSouressCodeMirrorJS" >> "$targetCodeMirrorJS")) and die "Failed to minify $derivedSouressCodeMirrorJS: $!";
    system(qq(python "$cssMinScript" < "$derivedSourcesCodeMirrorCSS" >> "$targetCodeMirrorCSS")) and die "Failed to minify $derivedSourcesCodeMirrorCSS: $!";

    # Copy over Main.html and the Images directory.
    copy($derivedSourcesMainHTML, File::Spec->catfile($targetResourcePath, 'Main.html'));

    ditto(File::Spec->catdir($uiRoot, 'Images'), File::Spec->catdir($targetResourcePath, 'Images'));

    # Copy the Legacy directory.
    ditto(File::Spec->catfile($uiRoot, 'Protocol', 'Legacy'), File::Spec->catfile($protocolDir, 'Legacy'));
} else {
    # Keep the files separate for engineering builds.
    ditto($uiRoot, $targetResourcePath);
}
