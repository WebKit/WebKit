#!/usr/bin/perl -w
#
# Copyright (C) 2005 Apple Computer, Inc.
# Copyright (C) 2006 Anders Carlsson <andersca@mac.com>
# 
# This file is part of WebKit
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU Library General Public License
# along with this library; see the file COPYING.LIB.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301, USA.
# 

# This script is a temporary hack. 
# Files are generated in the source directory, when they really should go
# to the DerivedSources directory.
# This should also eventually be a build rule driven off of .idl files
# however a build rule only solution is blocked by several radars:
# <rdar://problems/4251781&4251785>

use strict;

use File::Path;
use File::Basename;
use Getopt::Long;
use Cwd;

use IDLParser;
use CodeGenerator;

my @idlDirectories;
my $outputDirectory;
my $outputHeadersDirectory;
my $generator;
my $defines;
my $filename;
my $prefix;
my $preprocessor;
my $writeDependencies;
my $verbose;
my $supplementalDependencyFile;
my $additionalIdlFilesList;

GetOptions('include=s@' => \@idlDirectories,
           'outputDir=s' => \$outputDirectory,
           'outputHeadersDir=s' => \$outputHeadersDirectory,
           'generator=s' => \$generator,
           'defines=s' => \$defines,
           'filename=s' => \$filename,
           'prefix=s' => \$prefix,
           'preprocessor=s' => \$preprocessor,
           'verbose' => \$verbose,
           'write-dependencies' => \$writeDependencies,
           'supplementalDependencyFile=s' => \$supplementalDependencyFile,
           'additionalIdlFilesList=s' => \$additionalIdlFilesList);

my $targetIdlFile = $ARGV[0];

die('Must specify input file.') unless defined($targetIdlFile);
die('Must specify generator') unless defined($generator);
die('Must specify output directory.') unless defined($outputDirectory);

if (!$outputHeadersDirectory) {
    $outputHeadersDirectory = $outputDirectory;
}
$targetIdlFile = Cwd::realpath($targetIdlFile);
if ($verbose) {
    print "$generator: $targetIdlFile\n";
}
my $targetInterfaceName = fileparse(basename($targetIdlFile), ".idl");

my $idlFound = 0;
my @supplementedIdlFiles;
if ($supplementalDependencyFile) {
    # The format of a supplemental dependency file:
    #
    # DOMWindow.idl P.idl Q.idl R.idl
    # Document.idl S.idl
    # Event.idl
    # ...
    #
    # The above indicates that DOMWindow.idl is supplemented by P.idl, Q.idl and R.idl,
    # Document.idl is supplemented by S.idl, and Event.idl is supplemented by no IDLs.
    # The IDL that supplements another IDL (e.g. P.idl) never appears in the dependency file.
    open FH, "< $supplementalDependencyFile" or die "Cannot open $supplementalDependencyFile\n";
    while (my $line = <FH>) {
        my ($idlFile, @followingIdlFiles) = split(/\s+/, $line);
        if ($idlFile and basename($idlFile) eq basename($targetIdlFile)) {
            $idlFound = 1;
            @supplementedIdlFiles = @followingIdlFiles;
        }
    }
    close FH;

    # The file $additionalIdlFilesList contains one IDL file per line:
    # P.idl
    # Q.idl
    # ...
    # These IDL files are ones which should not be included in DerivedSources*.cpp
    # (i.e. they are not described in the supplemental dependency file)
    # but should generate .h and .cpp files.
    if (!$idlFound and $additionalIdlFilesList) {
        open FH, "< $additionalIdlFilesList" or die "Cannot open $additionalIdlFilesList\n";
        my @idlFiles = <FH>;
        chomp(@idlFiles);
        $idlFound = grep { $_ and basename($_) eq basename($targetIdlFile) } @idlFiles;
        close FH;
    }

    if (!$idlFound) {
        if ($verbose) {
            print "$targetIdlFile is supplementing another IDL file, and thus .h and .cpp for $targetIdlFile are not generated.\n";
        }
        exit 0;
    }
}

# Parse the target IDL file.
my $targetParser = IDLParser->new(!$verbose);
my $targetDocument = $targetParser->Parse($targetIdlFile, $defines, $preprocessor);

# FIXME(haraken): Remove this if-else statement.
# This if-else statement is temporary and will be removed
# after build scripts for all platforms support [Supplemental] IDL.
# The motivation for the [Supplemented] IDL is as follows:
#
# In order to support the [Supplemental] IDL, we need to
# (1) run resolve-supplemental.pl and generate supplemental_dependency.tmp
# (2) and run generate-bindings.pl with the supplemental_dependency.tmp.
#
# This build flow requires a change on the following build scripts,
# but changing all the build scripts all at once without any regression is too difficult:
#
#     - DerivedSources.make
#     - DerivedSources.pri
#     - GNUmakefile.am
#     - PlatformBlackBerry.cmake
#     - UseJSC.cmake
#     - UseV8.cmake
#     - WebCore.vcproj/MigrateScripts
#     - WebCore.vcproj/WebCore.vcproj
#     - bindings/gobject/GNUmakefile.am
#     - WebCore.gyp/WebCore.gyp
#
# Thus, we are planning to change the build scripts one by one, which implies that
# we need to allow the temporary state in which some build scripts support [Supplemental] IDL
# but others do not. To accomplish this, we introduce a temporal IDL, [Supplemented].
# The [Supplemented] IDL on an attribute means that the attribute is marked with [Supplemental]
# in another IDL file somewhere, like this:
#
# DOMWindowWebAudio.idl:
#     interface [
#         Supplemental=DOMWindow
#     ] DOMWindowWebAudio {
#         attribute attr1;
#         attribute attr2;
#     };
#
# DOMWindow.idl:
#     interface [
#     ] DOMWindow {
#         attribute [Supplemented] attr1; // This line will be removed after all build scripts support the [SupplementalL.
#         attribute [Supplemented] attr2; // This line will be removed after all build scripts support the [SupplementalL.
#         attribute attr3;
#         attribute attr4;
#     };
#
# Assuming these IDL files, the below code is doing the following logic:
#
#     - If a given build script supports the [Supplemental] IDL (i.e. --supplementalDependencyFile is specified),
#       we ignore all attributes with the [Supplemented] IDL.
#     - Otherwise (i.e. --supplementalDependencyFile is not specified),
#       we treat all attributes with the [Supplemented] IDL as normal attributes
#       and instead ignore all attributes with the [Supplemental] IDL
#       (i.e. we generate nothing from the idl file with the [Supplemental] IDL).
if ($supplementalDependencyFile) {
    foreach my $dataNode (@{$targetDocument->classes}) {
        my @nonSupplementedAttributes;
        foreach my $attribute (@{$dataNode->attributes}) {
            if (!$attribute->signature->extendedAttributes->{"Supplemented"}) {
                push(@nonSupplementedAttributes, $attribute);
            }
        }
        $dataNode->attributes(\@nonSupplementedAttributes);
    }
} else {
    foreach my $dataNode (@{$targetDocument->classes}) {
        if ($dataNode->extendedAttributes->{"Supplemental"}) {
            exit 0;
        }
    }
}
# Temporary if-else statement until here.

foreach my $idlFile (@supplementedIdlFiles) {
    next if $idlFile eq $targetIdlFile;

    my $interfaceName = fileparse(basename($idlFile), ".idl");
    my $parser = IDLParser->new(!$verbose);
    my $document = $parser->Parse($idlFile, $defines, $preprocessor);

    foreach my $dataNode (@{$document->classes}) {
        if ($dataNode->extendedAttributes->{"Supplemental"} and $dataNode->extendedAttributes->{"Supplemental"} eq $targetInterfaceName) {
            my $targetDataNode;
            foreach my $class (@{$targetDocument->classes}) {
                if ($class->name eq $targetInterfaceName) {
                    $targetDataNode = $class;
                    last;
                }
            }
            die "Not found an interface ${targetInterfaceName} in ${targetInterfaceName}.idl." unless defined $targetDataNode;

            foreach my $attribute (@{$dataNode->attributes}) {
                # Record that this attribute is implemented by $interfaceName.
                $attribute->signature->extendedAttributes->{"ImplementedBy"} = $interfaceName;

                # Add interface-wide extended attributes to each attribute.
                foreach my $extendedAttributeName (keys %{$dataNode->extendedAttributes}) {
                    next if ($extendedAttributeName eq "Supplemental");
                    $attribute->signature->extendedAttributes->{$extendedAttributeName} = $dataNode->extendedAttributes->{$extendedAttributeName};
                }
                push(@{$targetDataNode->attributes}, $attribute);
            }
        }
    }
}

# Generate desired output for the target IDL file.
my $codeGen = CodeGenerator->new(\@idlDirectories, $generator, $outputDirectory, $outputHeadersDirectory, 0, $preprocessor, $writeDependencies, $verbose);
$codeGen->ProcessDocument($targetDocument, $defines);
