#!/usr/bin/env perl
#
# Copyright (C) 2011 Google Inc.  All rights reserved.
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

use strict;
use warnings;
use FindBin;
use lib $FindBin::Bin;

use English;
use File::Basename;
use Getopt::Long;
use Cwd;
use Config;
use Class::Struct;
use JSON::PP;
use Data::Dumper;

use IDLParser;

my $defines;
my $preprocessor;
my $idlFileNamesList;
my $testGlobalContextName;
my $supplementalDependencyFile;
my $isoSubspacesHeaderFile;
my $windowConstructorsFile;
my $workerGlobalScopeConstructorsFile;
my $dedicatedWorkerGlobalScopeConstructorsFile;
my $serviceWorkerGlobalScopeConstructorsFile;
my $workletGlobalScopeConstructorsFile;
my $paintWorkletGlobalScopeConstructorsFile;
my $testGlobalScopeConstructorsFile;
my $supplementalMakefileDeps;
my $idlAttributesFile;

# Toggle this to validate that the fast regular expression based "parsing" used
# in this file produces the same results as the slower results produced by the
# complete IDLParser.
my $validateAgainstParser = 0;

GetOptions('defines=s' => \$defines,
           'preprocessor=s' => \$preprocessor,
           'idlFileNamesList=s' => \$idlFileNamesList,
           'testGlobalContextName=s' => \$testGlobalContextName,
           'supplementalDependencyFile=s' => \$supplementalDependencyFile,
           'isoSubspacesHeaderFile=s' => \$isoSubspacesHeaderFile,
           'windowConstructorsFile=s' => \$windowConstructorsFile,
           'workerGlobalScopeConstructorsFile=s' => \$workerGlobalScopeConstructorsFile,
           'dedicatedWorkerGlobalScopeConstructorsFile=s' => \$dedicatedWorkerGlobalScopeConstructorsFile,
           'serviceWorkerGlobalScopeConstructorsFile=s' => \$serviceWorkerGlobalScopeConstructorsFile,
           'workletGlobalScopeConstructorsFile=s' => \$workletGlobalScopeConstructorsFile,
           'paintWorkletGlobalScopeConstructorsFile=s' => \$paintWorkletGlobalScopeConstructorsFile,
           'testGlobalScopeConstructorsFile=s' => \$testGlobalScopeConstructorsFile,
           'supplementalMakefileDeps=s' => \$supplementalMakefileDeps,
           'idlAttributesFile=s' => \$idlAttributesFile,
           'validateAgainstParser' => \$validateAgainstParser);

die('Must specify #define macros using --defines.') unless defined($defines);
die('Must specify an output file using --supplementalDependencyFile.') unless defined($supplementalDependencyFile);
die('Must specify an output file using --windowConstructorsFile.') unless defined($windowConstructorsFile);
die('Must specify an output file using --workerGlobalScopeConstructorsFile.') unless defined($workerGlobalScopeConstructorsFile);
die('Must specify an output file using --dedicatedWorkerGlobalScopeConstructorsFile.') unless defined($dedicatedWorkerGlobalScopeConstructorsFile);
die('Must specify an output file using --serviceWorkerGlobalScopeConstructorsFile.') unless defined($serviceWorkerGlobalScopeConstructorsFile);
die('Must specify an output file using --workletGlobalScopeConstructorsFile.') unless defined($workletGlobalScopeConstructorsFile);
die('Must specify an output file using --paintWorkletGlobalScopeConstructorsFile.') unless defined($paintWorkletGlobalScopeConstructorsFile);
die('Must specify an output file using --testGlobalScopeConstructorsFile.') unless defined($testGlobalScopeConstructorsFile) || !defined($testGlobalContextName);
die('Must specify the file listing all IDLs using --idlFileNamesList.') unless defined($idlFileNamesList);
die('Must specify IDL attributes file using --idlAttributesFile.') unless defined($idlAttributesFile);

$supplementalDependencyFile = CygwinPathIfNeeded($supplementalDependencyFile);
$isoSubspacesHeaderFile = CygwinPathIfNeeded($isoSubspacesHeaderFile);
$windowConstructorsFile = CygwinPathIfNeeded($windowConstructorsFile);
$workerGlobalScopeConstructorsFile = CygwinPathIfNeeded($workerGlobalScopeConstructorsFile);
$dedicatedWorkerGlobalScopeConstructorsFile = CygwinPathIfNeeded($dedicatedWorkerGlobalScopeConstructorsFile);
$serviceWorkerGlobalScopeConstructorsFile = CygwinPathIfNeeded($serviceWorkerGlobalScopeConstructorsFile);
$workletGlobalScopeConstructorsFile = CygwinPathIfNeeded($workletGlobalScopeConstructorsFile);
$paintWorkletGlobalScopeConstructorsFile = CygwinPathIfNeeded($paintWorkletGlobalScopeConstructorsFile);
$supplementalMakefileDeps = CygwinPathIfNeeded($supplementalMakefileDeps) if defined($supplementalMakefileDeps);

my @idlFileNames;
open(my $fh, '<', $idlFileNamesList) or die "Cannot open $idlFileNamesList";
@idlFileNames = map { CygwinPathIfNeeded(s/\r?\n?$//r) } <$fh>;
close($fh) or die;

my $idlAttributes;
if ($validateAgainstParser) {
    my $input;
    {
        local $INPUT_RECORD_SEPARATOR;
        open(JSON, "<", $idlAttributesFile) or die "Couldn't open $idlAttributesFile: $!";
        $input = <JSON>;
        close(JSON);
    }

    my $jsonDecoder = JSON::PP->new->utf8;
    my $jsonHashRef = $jsonDecoder->decode($input);
    $idlAttributes = $jsonHashRef->{attributes};
}

struct( IDLFile => {
    fileName => '$',
    fileContents => '$',
    primaryDeclarationName => '$',
    parsedDocument => '$'
});

my %interfaceNameToIdlFilePath;
my %idlFilePathToInterfaceName;
my %supplementalDependencies;
my %dictionaryDependencies;
my %supplementals;
my $windowConstructorsCode = "";
my $workerGlobalScopeConstructorsCode = "";
my $dedicatedWorkerGlobalScopeConstructorsCode = "";
my $serviceWorkerGlobalScopeConstructorsCode = "";
my $workletGlobalScopeConstructorsCode = "";
my $paintWorkletGlobalScopeConstructorsCode = "";
my $testGlobalScopeConstructorsCode = "";

my $isoSubspacesHeaderCode = <<END;
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>

#pragma once

namespace WebCore {

class DOMIsoSubspaces {
    WTF_MAKE_NONCOPYABLE(DOMIsoSubspaces);
    WTF_MAKE_FAST_ALLOCATED(DOMIsoSubspaces);
public:
    DOMIsoSubspaces() = default;
END

# Get rid of duplicates in idlFileNames array.
my %idlFileNameHash = map { $_, 1 } @idlFileNames;

# Populate $idlFilePathToInterfaceName and $interfaceNameToIdlFilePath.
foreach my $idlFileName (sort keys %idlFileNameHash) {
    my $fullPath = Cwd::realpath($idlFileName);
    my $interfaceName = fileparse(basename($idlFileName), ".idl");
    $idlFilePathToInterfaceName{$fullPath} = $interfaceName;
    $interfaceNameToIdlFilePath{$interfaceName} = $fullPath;
}

# Parse all IDL files.
foreach my $idlFileName (sort keys %idlFileNameHash) {
    my $fullPath = Cwd::realpath($idlFileName);
    
    my $idlFile = processIDL($idlFileName, $fullPath);

    # Handle partial names.
    my $partialNames = getPartialNamesFromIDL($idlFile);
    if (@{$partialNames}) {
        $supplementalDependencies{$fullPath} = $partialNames;
        next;
    }

    $supplementals{$fullPath} = [];

    my $baseDictionaries = getUndefinedBaseDictionariesFromIDL($idlFile);
    if (@{$baseDictionaries}) {
        $dictionaryDependencies{$idlFile->primaryDeclarationName} = (join(".idl ", @{$baseDictionaries}) . ".idl");
    }

    # Skip if the IDL file does not contain an interface or a callback interface.
    # The IDL may contain a dictionary.
    next unless containsInterfaceOrCallbackInterfaceFromIDL($idlFile);

    my $interfaceName = $idlFile->primaryDeclarationName;
    # Handle include statements.
    my $includedInterfaces = getIncludedInterfacesFromIDL($idlFile, $interfaceName);
    foreach my $includedInterface (@{$includedInterfaces}) {
        my $includedIdlFilePath = $interfaceNameToIdlFilePath{$includedInterface};
        die "Could not find a the IDL file where the following included interface is defined: $includedInterface" unless $includedIdlFilePath;
        if ($supplementalDependencies{$includedIdlFilePath}) {
            push(@{$supplementalDependencies{$includedIdlFilePath}}, $interfaceName);
        } else {
            $supplementalDependencies{$includedIdlFilePath} = [$interfaceName];
        }
    }

    next if isMixinInterfaceFromIDL($idlFile);

    my $isCallbackInterface = isCallbackInterfaceFromIDL($idlFile);
    if (!$isCallbackInterface) {
        $isoSubspacesHeaderCode .= "    std::unique_ptr<JSC::IsoSubspace> m_subspaceFor${interfaceName};\n";
        if (containsIterableInterfaceFromIDL($idlFile)) {
            $isoSubspacesHeaderCode .= "    std::unique_ptr<JSC::IsoSubspace> m_subspaceFor${interfaceName}Iterator;\n";
        }
    }

    # For every interface that is exposed in a given ECMAScript global environment and:
    # - is a callback interface that has constants declared on it, or
    # - is a non-callback interface that is not declared with the [LegacyNoInterfaceObject] extended attribute, a corresponding
    #   property must exist on the ECMAScript environment's global object.
    # See https://heycam.github.io/webidl/#es-interfaces
    my $extendedAttributes = getInterfaceExtendedAttributesFromIDL($idlFile);
    if (shouldExposeInterface($extendedAttributes)) {
        if (!$isCallbackInterface || containsInterfaceWithConstantsFromIDL($idlFile)) {
            my $exposedAttribute = $extendedAttributes->{"Exposed"} || $testGlobalContextName || "Window";
            $exposedAttribute = substr($exposedAttribute, 1, -1) if substr($exposedAttribute, 0, 1) eq "(";
            my @globalContexts = split(",", $exposedAttribute);
            my ($attributeCode, $windowAliases) = GenerateConstructorAttributes($interfaceName, $extendedAttributes);
            foreach my $globalContext (@globalContexts) {
                if ($globalContext eq "Window") {
                    $windowConstructorsCode .= $attributeCode;
                } elsif ($globalContext eq "Worker") {
                    $workerGlobalScopeConstructorsCode .= $attributeCode;
                } elsif ($globalContext eq "DedicatedWorker") {
                    $dedicatedWorkerGlobalScopeConstructorsCode .= $attributeCode;
                } elsif ($globalContext eq "ServiceWorker") {
                    $serviceWorkerGlobalScopeConstructorsCode .= $attributeCode;
                } elsif ($globalContext eq "Worklet") {
                    $workletGlobalScopeConstructorsCode .= $attributeCode;
                } elsif ($globalContext eq "PaintWorklet") {
                    $paintWorkletGlobalScopeConstructorsCode .= $attributeCode;
                } elsif ($globalContext eq $testGlobalContextName) {
                    $testGlobalScopeConstructorsCode .= $attributeCode;
                } else {
                    die "Unsupported global context '$globalContext' used in [Exposed] at $idlFileName";
                }
            }
            $windowConstructorsCode .= $windowAliases if $windowAliases;
        }
    }
}

# Generate partial interfaces for Constructors.
GeneratePartialInterface("DOMWindow", $windowConstructorsCode, $windowConstructorsFile);
GeneratePartialInterface("WorkerGlobalScope", $workerGlobalScopeConstructorsCode, $workerGlobalScopeConstructorsFile);
GeneratePartialInterface("DedicatedWorkerGlobalScope", $dedicatedWorkerGlobalScopeConstructorsCode, $dedicatedWorkerGlobalScopeConstructorsFile);
GeneratePartialInterface("ServiceWorkerGlobalScope", $serviceWorkerGlobalScopeConstructorsCode, $serviceWorkerGlobalScopeConstructorsFile);
GeneratePartialInterface("WorkletGlobalScope", $workletGlobalScopeConstructorsCode, $workletGlobalScopeConstructorsFile);
GeneratePartialInterface("PaintWorkletGlobalScope", $paintWorkletGlobalScopeConstructorsCode, $paintWorkletGlobalScopeConstructorsFile);
GeneratePartialInterface($testGlobalContextName, $testGlobalScopeConstructorsCode, $testGlobalScopeConstructorsFile) if defined($testGlobalContextName);

# Resolves partial interfaces and include dependencies.
foreach my $idlFilePath (sort keys %supplementalDependencies) {
    my $baseFiles = $supplementalDependencies{$idlFilePath};
    foreach my $baseFile (@{$baseFiles}) {
        my $targetIdlFilePath = $interfaceNameToIdlFilePath{$baseFile} or die "${baseFile}.idl not found, but it is supplemented by $idlFilePath";
        push(@{$supplementals{$targetIdlFilePath}}, $idlFilePath);
    }
    delete $supplementals{$idlFilePath};
}

if ($isoSubspacesHeaderFile) {
    $isoSubspacesHeaderCode .= "};\n";
    $isoSubspacesHeaderCode .= "} // namespace WebCore\n";
    WriteFileIfChanged($isoSubspacesHeaderFile, $isoSubspacesHeaderCode);
}

# Outputs the dependency.
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
my $dependencies = "";
foreach my $idlFilePath (sort keys %supplementals) {
    $dependencies .= "$idlFilePath @{$supplementals{$idlFilePath}}\n";
}
WriteFileIfChanged($supplementalDependencyFile, $dependencies);

if ($supplementalMakefileDeps) {
    my $makefileDeps = "# Supplemental dependencies\n";
    foreach my $idlFilePath (sort keys %supplementals) {
        my $basename = $idlFilePathToInterfaceName{$idlFilePath};

        my @dependencyList = map { basename($_) } @{$supplementals{$idlFilePath}};
        my @dependencies = sort(keys %{{ map{$_=>1}@dependencyList}});

        $makefileDeps .= "JS${basename}.h: @{dependencies}\n";
        $makefileDeps .= "DOM${basename}.h: @{dependencies}\n";
        $makefileDeps .= "WebDOM${basename}.h: @{dependencies}\n";
        foreach my $dependency (@dependencies) {
            $makefileDeps .= "${dependency}:\n";
        }
    }

    $makefileDeps .= "# Dictionaries dependencies\n";
    foreach my $derivedDictionary (sort keys %dictionaryDependencies) {
        $makefileDeps .= "JS${derivedDictionary}.cpp: $dictionaryDependencies{$derivedDictionary}\n";
    }

    WriteFileIfChanged($supplementalMakefileDeps, $makefileDeps);
}

sub CygwinPathIfNeeded
{
    my $path = shift;
    return Cygwin::win_to_posix_path($path) if ($^O eq 'cygwin');
    return $path;
}

sub WriteFileIfChanged
{
    my $fileName = shift;
    my $contents = shift;

    if (-f $fileName) {
        open FH, "<", $fileName or die "Couldn't open $fileName: $!\n";
        my @lines = <FH>;
        my $oldContents = join "", @lines;
        close FH;
        return if $contents eq $oldContents;
    }
    open FH, ">", $fileName or die "Couldn't open $fileName: $!\n";
    print FH $contents;
    close FH;
}

sub GeneratePartialInterface
{
    my $interfaceName = shift;
    my $attributesCode = shift;
    my $destinationFile = shift;

    my $contents = "partial interface ${interfaceName} {\n$attributesCode};\n";
    WriteFileIfChanged($destinationFile, $contents);

    my $fullPath = Cwd::realpath($destinationFile);
    $supplementalDependencies{$fullPath} = [$interfaceName] if $interfaceNameToIdlFilePath{$interfaceName};
}

sub GenerateConstructorAttributes
{
    my $interfaceName = shift;
    my $extendedAttributes = shift;

    my $code = "    ";
    my @extendedAttributesList;
    foreach my $attributeName (sort keys %{$extendedAttributes}) {
      next unless ($attributeName eq "Conditional" || $attributeName eq "EnabledAtRuntime" || $attributeName eq "EnabledForWorld"
        || $attributeName eq "EnabledBySetting" || $attributeName eq "SecureContext" || $attributeName eq "PrivateIdentifier"
        || $attributeName eq "PublicIdentifier" || $attributeName eq "DisabledByQuirk" || $attributeName eq "EnabledByQuirk"
        || $attributeName eq "EnabledForContext" || $attributeName eq "CustomEnabled") || $attributeName eq "LegacyFactoryFunctionEnabledBySetting";
      my $extendedAttribute = $attributeName;
      $extendedAttribute .= "=" . $extendedAttributes->{$attributeName} unless $extendedAttributes->{$attributeName} eq "VALUE_IS_MISSING";
      push(@extendedAttributesList, $extendedAttribute);
    }
    $code .= "[" . join(', ', @extendedAttributesList) . "] " if @extendedAttributesList;

    my $originalInterfaceName = $interfaceName;
    $interfaceName = $extendedAttributes->{"InterfaceName"} if $extendedAttributes->{"InterfaceName"};
    $code .= "attribute " . $originalInterfaceName . "Constructor $interfaceName;\n";

    # In addition to the regular property, for every [LegacyFactoryFunction] extended attribute on an interface,
    # a corresponding property MUST exist on the ECMAScript global object.
    if ($extendedAttributes->{"LegacyFactoryFunction"}) {
        my $constructorName = $extendedAttributes->{"LegacyFactoryFunction"};
        $constructorName =~ s/\(.*//g; # Extract function name.
        $code .= "    ";
        $code .= "[" . join(', ', @extendedAttributesList) . "] " if @extendedAttributesList;
        $code .= "attribute " . $originalInterfaceName . "LegacyFactoryFunctionConstructor $constructorName;\n";
    }
    
    my $windowAliasesCode;
    if ($extendedAttributes->{"LegacyWindowAlias"}) {
        my $attributeValue = $extendedAttributes->{"LegacyWindowAlias"};
        $attributeValue = substr($attributeValue, 1, -1) if substr($attributeValue, 0, 1) eq "(";
        my @windowAliases = split(",", $attributeValue);
        foreach my $windowAlias (@windowAliases) {
            $windowAliasesCode .= "    ";
            $windowAliasesCode .= "[" . join(', ', @extendedAttributesList) . "] " if @extendedAttributesList;
            $windowAliasesCode .= "attribute " . $originalInterfaceName . "Constructor $windowAlias; // Legacy Window alias.\n";
        }
    }
    
    return ($code, $windowAliasesCode);
}

sub trim
{
    my $string = shift;
    $string =~ s/^\s+|\s+$//g;
    return $string;
}

sub listsAreIdentical
{
    my ($list1, $list2) = @_;
    
    if (scalar(@{$list1}) != scalar(@{$list2})) {
        return 0;
    }
    
    for my $i (0 .. $#{$list1}) {
        if ($list1->[$i] ne $list2->[$i]) {
            return 0;
        }
    }

    return 1;
}

sub processIDL
{
    my $fileName = shift;
    my $filePath = shift;

    my $idlFile = IDLFile->new();
    $idlFile->fileName($fileName);
    
    my $primaryDeclarationName = fileparse(basename($fileName), ".idl");
    $idlFile->primaryDeclarationName($primaryDeclarationName);

    open my $file, "<", $filePath or die "Could not open $idlFile for reading: $!";
    my @lines = <$file>;
    close $file;

    # FIXME: It would be useful to remove all c/c++ style comments as well to reduce false parses. Perhaps we can use preprocessor.pm if it is not too expensive.
    # Filter out preprocessor lines.
    @lines = grep(!/^\s*#/, @lines);

    $idlFile->fileContents(join('', @lines));

    if ($validateAgainstParser) {
        my $parser = IDLParser->new(1);
        $idlFile->parsedDocument($parser->Parse($filePath, $defines, $preprocessor, $idlAttributes));
    }

    return $idlFile;
}

sub getPartialNamesFromIDL
{
    my $idlFile = shift;

    my $fileContents = $idlFile->fileContents;

    my @partialNames = ();
    while ($fileContents =~ /partial\s+(interface|dictionary|namespace)\s+(\w+)/mg) {
        push(@partialNames, $2);
    }
    
    if ($validateAgainstParser) {
        print("Validating getPartialNamesFromIDL for " . $idlFile->fileName . " against validation parser.\n");

        my @partialsFromParsedDocument = ();
        foreach my $interface (@{$idlFile->parsedDocument->interfaces}) {
            push(@partialsFromParsedDocument, $interface->type->name) if $interface->isPartial;
        }
        foreach my $dictionary (@{$idlFile->parsedDocument->dictionaries}) {
            push(@partialsFromParsedDocument, $dictionary->type->name) if $dictionary->isPartial;
        }
        foreach my $namespace (@{$idlFile->parsedDocument->namespaces}) {
            push(@partialsFromParsedDocument, $namespace->name) if $namespace->isPartial;
        }

        my @sortedPartialNames = sort @partialNames;
        my @sortedPartialsFromParsedDocument = sort @partialsFromParsedDocument;

        local $Data::Dumper::Terse = 1;
        local $Data::Dumper::Indent = 0;
        unless (listsAreIdentical(\@sortedPartialNames, \@sortedPartialsFromParsedDocument)) {
            die "FAILURE: Partial declarations from regular expression based parser (" . Dumper(@sortedPartialNames) . ") don't match those from validation parser (" . Dumper(@sortedPartialsFromParsedDocument) . ") [" . $idlFile->fileName . "].";
        }
        print "SUCCESS! Partial declarations from regular expression based parser (" . Dumper(@sortedPartialNames) . ") match those from validation parser (" . Dumper(@sortedPartialsFromParsedDocument) . ").\n";
    }
    
    return \@partialNames;
}

# identifier-A includes identifier-B;
# https://heycam.github.io/webidl/#includes-statement
sub getIncludedInterfacesFromIDL
{
    my $idlFile = shift;
    my $interfaceName = shift;

    my $fileContents = $idlFile->fileContents;

    my @includedInterfaces = ();
    while ($fileContents =~ /^\s*(\w+)\s+includes\s+(\w+)\s*;/mg) {
        die "Identifier on the left of the 'includes' statement should be $interfaceName in $interfaceName.idl, but found $1" if $1 ne $interfaceName;
        push(@includedInterfaces, $2);
    }

    if ($validateAgainstParser) {
        print("Validating getIncludedInterfacesFromIDL for " . $idlFile->fileName . " against validation parser.\n");

        my @includedInterfacesFromParsedDocument = ();
        foreach my $include (@{$idlFile->parsedDocument->includes}) {
            push(@includedInterfacesFromParsedDocument, $include->mixinIdentifier);
        }

        my @sortedIncludedInterfaces = sort @includedInterfaces;
        my @sortedIncludedInterfacesFromParsedDocument = sort @includedInterfacesFromParsedDocument;

        local $Data::Dumper::Terse = 1;
        local $Data::Dumper::Indent = 0;

        unless (listsAreIdentical(\@sortedIncludedInterfaces, \@sortedIncludedInterfacesFromParsedDocument)) {
            die "FAILURE: Included interfaces from regular expression based parser (" . Dumper(@sortedIncludedInterfaces) . ") don't match those from validation parser (" . Dumper(@sortedIncludedInterfacesFromParsedDocument) . ") [" . $idlFile->fileName . "]";
        }
        print "SUCCESS! Included interfaces from regular expression based parser (" . Dumper(@sortedIncludedInterfaces) . ") match those from validation parser (" . Dumper(@sortedIncludedInterfacesFromParsedDocument) . ").\n";
    }

    return \@includedInterfaces
}

sub isCallbackInterfaceFromIDL
{
    my $idlFile = shift;

    my $fileContents = $idlFile->fileContents;
    my $containsCallbackInterface = ($fileContents =~ /callback\s+interface\s+\w+/gs);

    if ($validateAgainstParser) {
        print("Validating isCallbackInterfaceFromIDL for " . $idlFile->fileName . " against validation parser.\n");

        my $containsCallbackInterfaceFromParsedDocument = 0;
        foreach my $interface (@{$idlFile->parsedDocument->interfaces}) {
            if ($interface->isCallback) {
                $containsCallbackInterfaceFromParsedDocument = 1;
                last;
            }
        }

        unless ($containsCallbackInterface == $containsCallbackInterfaceFromParsedDocument ) {
            die "FAILURE: Determination of whether there is a callback interface from regular expression based parser (" . ($containsCallbackInterface ? "YES" : "NO") . ") doesn't match the determination from the validation parser (" . ($containsCallbackInterfaceFromParsedDocument ? "YES" : "NO") . ") [" . $idlFile->fileName . "].";
        }
        print "SUCCESS! Determination of whether there is a callback interface from regular expression based parser (" . ($containsCallbackInterface ? "YES" : "NO") . ") does match the determination from the validation parser (" . ($containsCallbackInterfaceFromParsedDocument ? "YES" : "NO") . ").\n";
    }

    return $containsCallbackInterface
}

sub isMixinInterfaceFromIDL
{
    my $idlFile = shift;

    my $fileContents = $idlFile->fileContents;
    my $containsMixinInterface = ($fileContents =~ /interface\s+mixin\s+\w+/gs);

    if ($validateAgainstParser) {
        print("Validating isCallbackInterfaceFromIDL for " . $idlFile->fileName . " against validation parser.\n");

        my $containsMixinInterfaceFromParsedDocument = 0;
        foreach my $interface (@{$idlFile->parsedDocument->interfaces}) {
            if ($interface->isMixin) {
                $containsMixinInterfaceFromParsedDocument = 1;
                last;
            }
        }

        unless ($containsMixinInterface == $containsMixinInterfaceFromParsedDocument ) {
            die "FAILURE: Determination of whether there is a mixin interface from regular expression based parser (" . ($containsMixinInterface ? "YES" : "NO") . ") doesn't match the determination from validation parser (" . ($containsMixinInterfaceFromParsedDocument ? "YES" : "NO") . ") [" . $idlFile->fileName . "].";
        }
        print "SUCCESS! Determination of whether there is a mixin interface from regular expression based parser (" . ($containsMixinInterface ? "YES" : "NO") . ") does match the determination from validation parser (" . ($containsMixinInterfaceFromParsedDocument ? "YES" : "NO") . ").\n";
    }

    return $containsMixinInterface;
}

sub containsIterableInterfaceFromIDL
{
    my $idlFile = shift;

    my $fileContents = $idlFile->fileContents;
    my $containsIterableInterface = ($fileContents =~ /iterable\s*<\s*\w+\s*/gs);

    if ($validateAgainstParser) {
        print("Validating containsIterableInterfaceFromIDL for " . $idlFile->fileName . " against validation parser.\n");

        my $containsIterableInterfaceFromParsedDocument = 0;
        foreach my $interface (@{$idlFile->parsedDocument->interfaces}) {
            if ($interface->iterable) {
                $containsIterableInterfaceFromParsedDocument = 1;
                last;
            }
        }

        unless ($containsIterableInterface == $containsIterableInterfaceFromParsedDocument ) {
            die "FAILURE: Determination of whether there is an iterable interface from regular expression based parser (" . ($containsIterableInterface ? "YES" : "NO") . ") doesn't match the determination from validation parser (" . ($containsIterableInterfaceFromParsedDocument ? "YES" : "NO") . ") [" . $idlFile->fileName . "].";
        }
        print "SUCCESS! Determination of whether there is an iterable interface from regular expression based parser (" . ($containsIterableInterface ? "YES" : "NO") . ") does match the determination from validation parser (" . ($containsIterableInterfaceFromParsedDocument ? "YES" : "NO") . ").\n";
    }

    return $containsIterableInterface;
}

sub containsInterfaceOrCallbackInterfaceFromIDL
{
    my $idlFile = shift;

    my $fileContents = $idlFile->fileContents;
    my $containsInterfaceOrCallbackInterface = ($fileContents =~ /\b(callback interface|interface)\s+(\w+)/gs);

    if ($validateAgainstParser) {
        print("Validating containsInterfaceOrCallbackInterfaceFromIDL for " . $idlFile->fileName . " against validation parser.\n");

        my $containsInterfaceOrCallbackInterfaceFromParsedDocument = (@{$idlFile->parsedDocument->interfaces} > 0);

        unless ($containsInterfaceOrCallbackInterface == $containsInterfaceOrCallbackInterfaceFromParsedDocument ) {
            die "FAILURE: Determination of whether there is an interface or callback interface from regular expression based parser (" . ($containsInterfaceOrCallbackInterface ? "YES" : "NO") . ") doesn't match the determination from validation parser (" . ($containsInterfaceOrCallbackInterfaceFromParsedDocument ? "YES" : "NO") . ") [" . $idlFile->fileName . "].";
        }
        print "SUCCESS! Determination of whether there is an interface or callback interface from regular expression based parser (" . ($containsInterfaceOrCallbackInterface ? "YES" : "NO") . ") does match the determination from validation parser (" . ($containsInterfaceOrCallbackInterfaceFromParsedDocument ? "YES" : "NO") . ").\n";
    }

    return $containsInterfaceOrCallbackInterface;
}

sub containsInterfaceWithConstantsFromIDL
{
    my $idlFile = shift;

    my $fileContents = $idlFile->fileContents;
    my $containsInterfaceWithConstants = ($fileContents =~ /\s+const[\s\w]+=\s+[\w]+;/gs);

    if ($validateAgainstParser) {
        print("Validating containsInterfaceWithConstantsFromIDL for " . $idlFile->fileName . " against validation parser.\n");

        my $containsInterfaceWithConstantsFromParsedDocument = 0;
        foreach my $interface (@{$idlFile->parsedDocument->interfaces}) {
            if (@{$interface->constants} > 0) {
                $containsInterfaceWithConstantsFromParsedDocument = 1;
                last;
            }
        }

        unless ($containsInterfaceWithConstants == $containsInterfaceWithConstantsFromParsedDocument ) {
            die "FAILURE: Determination of whether there is an interface with constants from regular expression based parser (" . ($containsInterfaceWithConstants ? "YES" : "NO") . ") doesn't match the determination from validation parser (" . ($containsInterfaceWithConstantsFromParsedDocument ? "YES" : "NO") . ") [" . $idlFile->fileName . "].";
        }
        print "SUCCESS! Determination of whether there is an interface with constants from regular expression based parser (" . ($containsInterfaceWithConstants ? "YES" : "NO") . ") does match the determination from validation parser (" . ($containsInterfaceWithConstantsFromParsedDocument ? "YES" : "NO") . ").\n";
    }

    return $containsInterfaceWithConstants;
}

sub getInterfaceExtendedAttributesFromIDL
{
    my $idlFile = shift;

    my $fileContents = $idlFile->fileContents;

    my $extendedAttributes = {};

    # Remove comments from fileContents before processing.
    # FIX: Preference to use Regex::Common::comment, however it is not available on
    # all build systems.
    $fileContents =~ s/(?:(?:(?:\/\/)(?:[^\n]*)(?:\n))|(?:(?:\/\*)(?:(?:[^\*]+|\*(?!\/))*)(?:\*\/)))//g;

    if ($fileContents =~ /\[(.*)\]\s+(callback interface|interface)\s+(\w+)/gs) {
        my $parameters = $1;
        if (index($parameters, '}') != -1) {
            # In case we have a declaration like a dictionary with extended attributes defined before the interface.
            $parameters = (split '}', $1)[-1];
            $parameters = substr($parameters, index($parameters, '[') + 1);
        }
        my @parts = split(m/,(?![^()]*\))/, $parameters);
        foreach my $part (@parts) {
            my @keyValue = split('=', $part);
            my $key = trim($keyValue[0]);
            next unless length($key);
            my $value = "VALUE_IS_MISSING";
            $value = trim($keyValue[1]) if @keyValue > 1;
            $extendedAttributes->{$key} = $value;
        }
    }

    if ($validateAgainstParser) {
        print("Validating getInterfaceExtendedAttributesFromIDL for " . $idlFile->fileName . " against validation parser.\n");

        my $primaryInterface;
        foreach my $interface (@{$idlFile->parsedDocument->interfaces}) {
            if ($interface->type->name eq $idlFile->primaryDeclarationName) {
                $primaryInterface = $interface;
                last;
            }
        }

        # FIXME: Comparing the deep structure of the extended attributes is suitably complex that for now
        # we only validate that both parsers produce the same keys.

        my @sortedExtendedAttributeKeys = sort keys %{$extendedAttributes};
        my @sortedExtendedAttributeKeysFromParsedDocument = sort keys %{$primaryInterface->extendedAttributes};

        local $Data::Dumper::Terse = 1;
        local $Data::Dumper::Indent = 0;

        unless (listsAreIdentical(\@sortedExtendedAttributeKeys, \@sortedExtendedAttributeKeysFromParsedDocument)) {
            die "FAILURE: Extended attributes for the primary interface from regular expression based parser (" . Dumper(@sortedExtendedAttributeKeys) . ") don't match those from validation parser (" . Dumper(@sortedExtendedAttributeKeysFromParsedDocument) . ") [" . $idlFile->fileName . "]";
        }
        print "SUCCESS! Extended attributes for the primary interface from regular expression based parser (" . Dumper(@sortedExtendedAttributeKeys) . ") match those from validation parser (" . Dumper(@sortedExtendedAttributeKeysFromParsedDocument) . ").\n";
    }

    return $extendedAttributes;
}

sub getUndefinedBaseDictionariesFromIDL
{
    my $idlFile = shift;

    my $fileContents = $idlFile->fileContents;

    my @dictionaryNames = ();
    while ($fileContents =~ /\s*dictionary\s+(\w+)\s*/mg) {
        push(@dictionaryNames, $1)
    }

    $fileContents = $idlFile->fileContents;
    my @baseDictionaries = ();
    while ($fileContents =~ /\s*dictionary\s+(\w+)\s+:\s+(\w+)\s*/mg) {
        next if (grep { $_ eq $2 } @dictionaryNames);
        next if (grep { $_ eq $2 } @baseDictionaries);
        push(@baseDictionaries, $2);
    }

    if ($validateAgainstParser) {
        print("Validating getUndefinedBaseDictionariesFromIDL for " . $idlFile->fileName . " against validation parser.\n");

        my @dictionaryNamesFromParsedDocument = ();
        foreach my $dictionary (@{$idlFile->parsedDocument->dictionaries}) {
            push(@dictionaryNamesFromParsedDocument, $dictionary->type->name);
        }

        my @baseDictionariesFromParsedDocument = ();
        foreach my $dictionary (@{$idlFile->parsedDocument->dictionaries}) {
            # Skip dictionaries without a base type.
            next if !$dictionary->parentType;
            # Skip dictionaries that have their base type defined in this document.
            next if (grep { $_ eq $dictionary->parentType->name } @dictionaryNamesFromParsedDocument);
            # Skip dictionaries that have already been added to the list.
            next if (grep { $_ eq $dictionary->parentType->name } @baseDictionariesFromParsedDocument);

            push(@baseDictionariesFromParsedDocument, $dictionary->parentType->name);
        }

        my @sortedBaseDictionaries = sort @baseDictionaries;
        my @sortedBaseDictionariesFromParsedDocument = sort @baseDictionariesFromParsedDocument;

        local $Data::Dumper::Terse = 1;
        local $Data::Dumper::Indent = 0;

        unless (listsAreIdentical(\@sortedBaseDictionaries, \@sortedBaseDictionariesFromParsedDocument)) {
            die "FAILURE: Undefined base dictionaries from regular expression based parser (" . Dumper(@sortedBaseDictionaries) . ") don't match those from validation parser (" . Dumper(@sortedBaseDictionariesFromParsedDocument) . ") [" . $idlFile->fileName . "]";
        }
        print "SUCCESS! Undefined base dictionaries from regular expression based parser (" . Dumper(@sortedBaseDictionaries) . ") match those from validation parser (" . Dumper(@sortedBaseDictionariesFromParsedDocument) . ").\n";
    }

    return \@baseDictionaries;
}

sub shouldExposeInterface
{
    my $extendedAttributes = shift;

    return !$extendedAttributes->{"LegacyNoInterfaceObject"};
}
