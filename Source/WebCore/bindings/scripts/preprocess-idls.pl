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

use File::Basename;
use Getopt::Long;
use Cwd;
use Config;

my $defines;
my $preprocessor;
my $idlFilesList;
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

GetOptions('defines=s' => \$defines,
           'preprocessor=s' => \$preprocessor,
           'idlFilesList=s' => \$idlFilesList,
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
           'supplementalMakefileDeps=s' => \$supplementalMakefileDeps);

die('Must specify #define macros using --defines.') unless defined($defines);
die('Must specify an output file using --supplementalDependencyFile.') unless defined($supplementalDependencyFile);
die('Must specify an output file using --windowConstructorsFile.') unless defined($windowConstructorsFile);
die('Must specify an output file using --workerGlobalScopeConstructorsFile.') unless defined($workerGlobalScopeConstructorsFile);
die('Must specify an output file using --dedicatedWorkerGlobalScopeConstructorsFile.') unless defined($dedicatedWorkerGlobalScopeConstructorsFile);
die('Must specify an output file using --serviceWorkerGlobalScopeConstructorsFile.') unless defined($serviceWorkerGlobalScopeConstructorsFile);
die('Must specify an output file using --workletGlobalScopeConstructorsFile.') unless defined($workletGlobalScopeConstructorsFile);
die('Must specify an output file using --paintWorkletGlobalScopeConstructorsFile.') unless defined($paintWorkletGlobalScopeConstructorsFile);
die('Must specify an output file using --testGlobalScopeConstructorsFile.') unless defined($testGlobalScopeConstructorsFile) || !defined($testGlobalContextName);
die('Must specify the file listing all IDLs using --idlFilesList.') unless defined($idlFilesList);

$supplementalDependencyFile = CygwinPathIfNeeded($supplementalDependencyFile);
$isoSubspacesHeaderFile = CygwinPathIfNeeded($isoSubspacesHeaderFile);
$windowConstructorsFile = CygwinPathIfNeeded($windowConstructorsFile);
$workerGlobalScopeConstructorsFile = CygwinPathIfNeeded($workerGlobalScopeConstructorsFile);
$dedicatedWorkerGlobalScopeConstructorsFile = CygwinPathIfNeeded($dedicatedWorkerGlobalScopeConstructorsFile);
$serviceWorkerGlobalScopeConstructorsFile = CygwinPathIfNeeded($serviceWorkerGlobalScopeConstructorsFile);
$workletGlobalScopeConstructorsFile = CygwinPathIfNeeded($workletGlobalScopeConstructorsFile);
$paintWorkletGlobalScopeConstructorsFile = CygwinPathIfNeeded($paintWorkletGlobalScopeConstructorsFile);
$supplementalMakefileDeps = CygwinPathIfNeeded($supplementalMakefileDeps) if defined($supplementalMakefileDeps);

my @idlFiles;
open(my $fh, '<', $idlFilesList) or die "Cannot open $idlFilesList";
@idlFiles = map { CygwinPathIfNeeded(s/\r?\n?$//r) } <$fh>;
close($fh) or die;

my %interfaceNameToIdlFile;
my %idlFileToInterfaceName;
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

# Get rid of duplicates in idlFiles array.
my %idlFileHash = map { $_, 1 } @idlFiles;

# Populate $idlFileToInterfaceName and $interfaceNameToIdlFile.
foreach my $idlFile (sort keys %idlFileHash) {
    my $fullPath = Cwd::realpath($idlFile);
    my $interfaceName = fileparse(basename($idlFile), ".idl");
    $idlFileToInterfaceName{$fullPath} = $interfaceName;
    $interfaceNameToIdlFile{$interfaceName} = $fullPath;
}

# Parse all IDL files.
foreach my $idlFile (sort keys %idlFileHash) {
    my $fullPath = Cwd::realpath($idlFile);
    my $idlFileContents = getFileContents($fullPath);
    # Handle partial names.
    my $partialNames = getPartialNamesFromIDL($idlFileContents);
    if (@{$partialNames}) {
        $supplementalDependencies{$fullPath} = $partialNames;
        next;
    }

    $supplementals{$fullPath} = [];

    updateDictionaryDependencies($idlFileContents, $idlFile);

    # Skip if the IDL file does not contain an interface, a callback interface or an exception.
    # The IDL may contain a dictionary.
    next unless containsInterfaceOrExceptionFromIDL($idlFileContents);

    my $interfaceName = fileparse(basename($idlFile), ".idl");
    # Handle include statements.
    my $includedInterfaces = getIncludedInterfacesFromIDL($idlFileContents, $interfaceName);
    foreach my $includedInterface (@{$includedInterfaces}) {
        my $includedIdlFile = $interfaceNameToIdlFile{$includedInterface};
        die "Could not find a the IDL file where the following included interface is defined: $includedInterface" unless $includedIdlFile;
        if ($supplementalDependencies{$includedIdlFile}) {
            push(@{$supplementalDependencies{$includedIdlFile}}, $interfaceName);
        } else {
            $supplementalDependencies{$includedIdlFile} = [$interfaceName];
        }
    }

    if (!isCallbackInterfaceFromIDL($idlFileContents)) {
        $isoSubspacesHeaderCode .= "    std::unique_ptr<JSC::IsoSubspace> m_subspaceFor${interfaceName};\n";
        if (interfaceIsIterable($idlFileContents)) {
            $isoSubspacesHeaderCode .= "    std::unique_ptr<JSC::IsoSubspace> m_subspaceFor${interfaceName}Iterator;\n";
        }
    }

    # For every interface that is exposed in a given ECMAScript global environment and:
    # - is a callback interface that has constants declared on it, or
    # - is a non-callback interface that is not declared with the [LegacyNoInterfaceObject] extended attribute, a corresponding
    #   property must exist on the ECMAScript environment's global object.
    # See https://heycam.github.io/webidl/#es-interfaces
    my $extendedAttributes = getInterfaceExtendedAttributesFromIDL($idlFileContents);
    if (shouldExposeInterface($extendedAttributes)) {
        if (!isCallbackInterfaceFromIDL($idlFileContents) || interfaceHasConstantAttribute($idlFileContents)) {
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
                    die "Unsupported global context '$globalContext' used in [Exposed] at $idlFile";
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
foreach my $idlFile (sort keys %supplementalDependencies) {
    my $baseFiles = $supplementalDependencies{$idlFile};
    foreach my $baseFile (@{$baseFiles}) {
        my $targetIdlFile = $interfaceNameToIdlFile{$baseFile} or die "${baseFile}.idl not found, but it is supplemented by $idlFile";
        push(@{$supplementals{$targetIdlFile}}, $idlFile);
    }
    delete $supplementals{$idlFile};
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
foreach my $idlFile (sort keys %supplementals) {
    $dependencies .= "$idlFile @{$supplementals{$idlFile}}\n";
}
WriteFileIfChanged($supplementalDependencyFile, $dependencies);

if ($supplementalMakefileDeps) {
    my $makefileDeps = "# Supplemental dependencies\n";
    foreach my $idlFile (sort keys %supplementals) {
        my $basename = $idlFileToInterfaceName{$idlFile};

        my @dependencyList = map { basename($_) } @{$supplementals{$idlFile}};
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
    $supplementalDependencies{$fullPath} = [$interfaceName] if $interfaceNameToIdlFile{$interfaceName};
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

sub getFileContents
{
    my $idlFile = shift;

    open my $file, "<", $idlFile or die "Could not open $idlFile for reading: $!";
    my @lines = <$file>;
    close $file;

    # Filter out preprocessor lines.
    @lines = grep(!/^\s*#/, @lines);

    return join('', @lines);
}

sub getPartialNamesFromIDL
{
    my $fileContents = shift;
    my @partialNames = ();
    while ($fileContents =~ /partial\s+(interface|dictionary)\s+(\w+)/mg) {
        push(@partialNames, $2);
    }
    return \@partialNames;
}

# identifier-A includes identifier-B;
# https://heycam.github.io/webidl/#includes-statement
sub getIncludedInterfacesFromIDL
{
    my $fileContents = shift;
    my $interfaceName = shift;

    my @includedInterfaces = ();
    while ($fileContents =~ /^\s*(\w+)\s+includes\s+(\w+)\s*;/mg) {
        die "Identifier on the left of the 'includes' statement should be $interfaceName in $interfaceName.idl, but found $1" if $1 ne $interfaceName;
        push(@includedInterfaces, $2);
    }
    return \@includedInterfaces
}

sub isCallbackInterfaceFromIDL
{
    my $fileContents = shift;
    return ($fileContents =~ /callback\s+interface\s+\w+/gs);
}

sub interfaceIsIterable
{
    my $fileContents = shift;
    return ($fileContents =~ /iterable\s*<\s*\w+\s*/gs);
}

sub containsInterfaceOrExceptionFromIDL
{
    my $fileContents = shift;

    return 1 if $fileContents =~ /\bcallback\s+interface\s+\w+/gs;
    return 1 if $fileContents =~ /\binterface\s+\w+/gs;
    return 1 if $fileContents =~ /\bexception\s+\w+/gs;
    return 0;
}

sub trim
{
    my $string = shift;
    $string =~ s/^\s+|\s+$//g;
    return $string;
}

sub getInterfaceExtendedAttributesFromIDL
{
    my $fileContents = shift;

    my $extendedAttributes = {};

    # Remove comments from fileContents before processing.
    # FIX: Preference to use Regex::Common::comment, however it is not available on
    # all build systems.
    $fileContents =~ s/(?:(?:(?:\/\/)(?:[^\n]*)(?:\n))|(?:(?:\/\*)(?:(?:[^\*]+|\*(?!\/))*)(?:\*\/)))//g;

    if ($fileContents =~ /\[(.*)\]\s+(callback interface|interface|exception)\s+(\w+)/gs) {
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

    return $extendedAttributes;
}

sub interfaceHasConstantAttribute
{
    my $fileContents = shift;

    return $fileContents =~ /\s+const[\s\w]+=\s+[\w]+;/gs;
}

sub shouldExposeInterface
{
    my $extendedAttributes = shift;

    return !$extendedAttributes->{"LegacyNoInterfaceObject"};
}

sub updateDictionaryDependencies
{
    my $fileContents = shift;
    my $idlFile = shift;

    my $dictionaryName = fileparse(basename($idlFile), ".idl");

    my @baseDictionaries = ();
    while ($fileContents =~ /\s*dictionary\s+(\w+)\s+:\s+(\w+)\s*/mg) {
        next if !$interfaceNameToIdlFile{$2} || (grep { $_ eq $2 } @baseDictionaries);
        push(@baseDictionaries, $2);
    }
    $dictionaryDependencies{$dictionaryName} = (join(".idl ", @baseDictionaries) . ".idl") if @baseDictionaries;
}
