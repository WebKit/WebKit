#!/usr/bin/env perl

# Copyright (C) 2005-2007, 2009, 2013-2014 Apple Inc. All rights reserved.
# Copyright (C) 2009, Julien Chaffraix <jchaffraix@webkit.org>
# Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
# Copyright (C) 2011 Ericsson AB. All rights reserved.
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
use FindBin;
use lib "$FindBin::Bin/../bindings/scripts";

use StaticString;
use Config;
use Getopt::Long;
use File::Path;
use File::Spec;
use IO::File;
use InFilesParser;

sub readElements($$);
sub readAttrs($$);

my $printFactory = 0; 
my $printEnum = "";
my $printWrapperFactory = 0; 
my $fontNamesIn = "";
my @elementsFiles = ();
my @attrsFiles = ();
my $outputDir = ".";
my %allElements = ();
my %allAttrs = ();
my %allCppNamespaces = ();
my %allElementsPerNamespace = ();
my %allAttrsPerNamespace = ();
my %allNamespacesPerElementLocalName = ();
my %parameters = ();
my $initDefaults = 1;

require Config;

my $ccLocation = "";
if ($ENV{CC}) {
    $ccLocation = $ENV{CC};
} elsif ($Config::Config{"osname"} eq "darwin" && $ENV{SDKROOT}) {
    chomp($ccLocation = `xcrun -find cc -sdk '$ENV{SDKROOT}'`);
} else {
    $ccLocation = "/usr/bin/cc";
}

GetOptions(
    'elements=s' => \@elementsFiles,
    'attrs=s' => \@attrsFiles,
    'factory' => \$printFactory,
    'outputDir=s' => \$outputDir,
    'wrapperFactory' => \$printWrapperFactory,
    'fonts=s' => \$fontNamesIn,
    'enum=s' => \$printEnum
);

mkpath($outputDir);

if (length($fontNamesIn)) {
    my $names = new IO::File;
    my $familyNamesFileBase = "WebKitFontFamily";

    open($names, $fontNamesIn) or die "Failed to open file: $fontNamesIn";

    $initDefaults = 0;
    my $Parser = InFilesParser->new();
    my $dummy;
    $Parser->parse($names, \&parametersHandler, \&dummy);

    my $F;
    my $header = File::Spec->catfile($outputDir, "${familyNamesFileBase}Names.h");
    open F, ">$header" or die "Unable to open $header for writing.";

    printLicenseHeader($F);
    printHeaderHead($F, "CSS", $familyNamesFileBase, <<END, "");
#include <wtf/NeverDestroyed.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>
END

    print F "enum class FamilyNamesIndex {\n";
    for my $name (sort keys %parameters) {
        print F "    ", ucfirst(${name}), ",\n";
    }
    print F "};\n\n";

    print F "template<typename T, size_t inlineCapacity = 0>\n";
    print F "class FamilyNamesList : public Vector<T, inlineCapacity> {\n";
    print F "public:\n";
    print F "    T& at(FamilyNamesIndex i)\n";
    print F "    {\n";
    print F "        return Vector<T, inlineCapacity>::at(static_cast<size_t>(i));\n";
    print F "    }\n";
    print F "};\n\n";
    print F "extern LazyNeverDestroyed<FamilyNamesList<const StaticStringImpl*, ", scalar(keys %parameters), ">> familyNamesData;\n";
    print F "extern MainThreadLazyNeverDestroyed<FamilyNamesList<AtomStringImpl*, ", scalar(keys %parameters), ">> familyNames;\n\n";
    printMacros($F, "extern MainThreadLazyNeverDestroyed<const AtomString>", "", \%parameters);
    print F "\n";
    print F "\n";

    printInit($F, 1);
    close F;

    my $source = File::Spec->catfile($outputDir, "${familyNamesFileBase}Names.cpp");
    open F, ">$source" or die "Unable to open $source for writing.";

    printLicenseHeader($F);
    printCppHead($F, "CSS", $familyNamesFileBase, "", "WTF");

    print F StaticString::GenerateStrings(\%parameters);

    print F "LazyNeverDestroyed<FamilyNamesList<const StaticStringImpl*, ", scalar(keys %parameters), ">> familyNamesData;\n";
    print F "MainThreadLazyNeverDestroyed<FamilyNamesList<AtomStringImpl*, ", scalar(keys %parameters), ">> familyNames;\n\n";

    printMacros($F, "MainThreadLazyNeverDestroyed<const AtomString>", "", \%parameters);

    printInit($F, 0);

    print F "\n";
    print F StaticString::GenerateStringAsserts(\%parameters);

    print F "    familyNamesData.construct();\n";
    for my $name (sort keys %parameters) {
        print F "    familyNamesData->uncheckedAppend(&${name}Data);\n";
    }

    print F "\n";
    for my $name (sort keys %parameters) {
        print F "    ${name}.construct(&${name}Data);\n";
    }

    print F "\n";
    print F "    familyNames.construct();\n";
    for my $name (sort keys %parameters) {
        print F "    familyNames->uncheckedAppend(${name}->impl());\n";
    }

    print F "}\n}\n}\n";
    close F;
    exit 0;
}

die "You must specify at least one of --elements <file> or --attrs <file>" unless (@elementsFiles || @attrsFiles);
die "You must not specify multiple --elements <file> arguments unless --domNames is also specified" if $printEnum eq "" && @elementsFiles > 1;
die "You must not specify multiple --attrs <file> arguments unless --domNames is also specified" if $printEnum eq "" && @attrsFiles > 1;
die "--enum must not be specified with --factory, --wrapperFactory, or --fonts" if $printEnum ne "" && ($printFactory || $printWrapperFactory || length($fontNamesIn));
die "Unsupported value for --enum" if $printEnum !~ /^(:?TagName|ElementName|Namespace)?$/;

for my $elementsFile (@elementsFiles) {
    readElements(\%allElements, $elementsFile);
    %parameters = () if $printEnum ne "";
}

for my $attrsFile (@attrsFiles) {
    readAttrs(\%allAttrs, $attrsFile);
    %parameters = () if $printEnum ne "";
}

if ($printEnum eq "") {
    $parameters{fallbackJSInterfaceName} = $parameters{fallbackInterfaceName} unless $parameters{fallbackJSInterfaceName};
}

collectAllElementsAndAttrsPerNamespace();

if ($printEnum eq "") {
    my $typeHelpersBasePath = "$outputDir/$parameters{namespace}ElementTypeHelpers";
    my $namesBasePath = "$outputDir/$parameters{namespace}Names";

    printNamesHeaderFile("$namesBasePath.h");
    printNamesCppFile("$namesBasePath.cpp");
    printTypeHelpersHeaderFile("$typeHelpersBasePath.h");
}

if ($printFactory) {
    my $factoryBasePath = "$outputDir/$parameters{namespace}ElementFactory";

    printFactoryCppFile("$factoryBasePath.cpp");
    printFactoryHeaderFile("$factoryBasePath.h");
}

if ($printWrapperFactory) {
    my $wrapperFactoryFileName = "$parameters{namespace}ElementWrapperFactory";

    printWrapperFactoryCppFile($outputDir, $wrapperFactoryFileName);
    printWrapperFactoryHeaderFile($outputDir, $wrapperFactoryFileName);
}

if ($printEnum eq "TagName") {
    printTagNameHeaderFile("$outputDir/TagName.h");
    printTagNameCppFile("$outputDir/TagName.cpp");
}

if ($printEnum eq "ElementName") {
    printElementNameHeaderFile("$outputDir/ElementName.h");
    printElementNameCppFile("$outputDir/ElementName.cpp");
}

if ($printEnum eq "Namespace") {
    printNamespaceHeaderFile("$outputDir/Namespace.h");
    printNamespaceCppFile("$outputDir/Namespace.cpp");
}

### Hash initialization

sub defaultElementPropertyHash
{
    my $localName = shift;

    my $identifier = $localName =~ s/-/_/gr;
    my $requiresAdjustment = $localName ne lc $localName;
    my $tagEnumValue = avoidConflictingName($identifier);

    return (
        cppNamespace => $parameters{namespace},
        namespace => $parameters{namespace},
        localName => $localName,
        identifier => $identifier,
        tagEnumValue => $tagEnumValue,
        elementEnumValue => "$parameters{namespace}_$identifier",
        unadjustedTagEnumValue => $requiresAdjustment ? lc($identifier) . "CaseUnadjusted" : "",
        parsedTagName => lc $localName,
        parsedTagEnumValue => $requiresAdjustment ? lc($identifier) . "CaseUnadjusted" : $tagEnumValue,
        constructorNeedsCreatedByParser => 0,
        constructorNeedsFormElement => 0,
        noConstructor => 0,
        interfaceName => defaultInterfaceName($identifier),
        # By default, the JSInterfaceName is the same as the interfaceName.
        JSInterfaceName => defaultInterfaceName($identifier),
        wrapperOnlyIfMediaIsAvailable => 0,
        settingsConditional => 0,
        conditional => 0,
        deprecatedGlobalSettingsConditional => 0,
        customTypeHelper => 0,
    );
}

sub defaultAttrPropertyHash
{
    my $localName = shift;

    my $identifier = $localName =~ s/^x-webkit-/webkit/r =~ s/-/_/gr;

    return (
        cppNamespace => $parameters{namespace},
        namespace => $parameters{attrsNullNamespace} ? "" : $parameters{namespace},
        localName => $localName,
        identifier => $identifier,
    );
}

sub defaultParametersHash
{
    return (
        namespace => '',
        namespacePrefix => '',
        namespaceURI => '',
        guardFactoryWith => '',
        elementsNullNamespace => 0,
        attrsNullNamespace => 0,
        fallbackInterfaceName => '',
        fallbackJSInterfaceName => '',
        customElementInterfaceName => '',
    );
}

sub defaultInterfaceName
{
    die "No namespace found" if !$parameters{namespace};
    return $parameters{namespace} . upperCaseName($_[0]) . "Element"
}

### Parsing handlers

sub avoidConflictingName
{
    my $name = shift;

    # C++ keywords that we know conflict with element names. Also "small", which is a macro defined in Windows headers.
    my %namesToAvoid = map { $_ => 1 } qw(small switch template);
    $name .= "_" if $namesToAvoid{$name};

    return $name;
}

sub makeKey
{
    my $namespace = shift;
    my $localName = shift;

    return "$namespace:$localName";
}

sub elementsHandler
{
    my ($element, $property, $value) = @_;

    my $elementKey = makeKey($parameters{namespace}, $element);

    # Initialize default property values.
    $allElements{$elementKey} = { defaultElementPropertyHash($element) } if !defined($allElements{$elementKey});

    if ($property) {
        die "Unknown property $property for element $element\n" if !defined($allElements{$elementKey}{$property});

        # The code relies on JSInterfaceName deriving from interfaceName to check for custom JSInterfaceName.
        # So override JSInterfaceName if it was not already set.
        $allElements{$elementKey}{JSInterfaceName} = $value if $property eq "interfaceName" && $allElements{$elementKey}{JSInterfaceName} eq $allElements{$elementKey}{interfaceName};

        $allElements{$elementKey}{$property} = $value;
    }
}

sub attrsHandler
{
    my ($attr, $property, $value) = @_;

    my $attrKey = makeKey($parameters{namespace}, $attr);

    # Initialize default property values.
    $allAttrs{$attrKey} = { defaultAttrPropertyHash($attr) } if !defined($allAttrs{$attrKey});

    if ($property) {
        die "Unknown property $property for attribute $attr\n" if !defined($allAttrs{$attrKey}{$property});
        $allAttrs{$attrKey}{$property} = $value;
    }
}

sub parametersHandler
{
    my ($parameter, $value) = @_;

    # Initialize default properties' values.
    %parameters = defaultParametersHash() if (!(keys %parameters) && $initDefaults);

    die "Unknown parameter $parameter for tags/attrs\n" if (!defined($parameters{$parameter}) && $initDefaults);
    $parameters{$parameter} = $value;
}

## Support routines

sub readNames($$$)
{
    my ($namesFile, $hashToFillRef, $handler) = @_;

    my $names = new IO::File;
    open($names, $namesFile) or die "Failed to open file: $namesFile";

    my $InParser = InFilesParser->new();
    $InParser->parse($names, \&parametersHandler, $handler);

    die "You must specify a namespace (e.g. SVG) for <namespace>Names.h" unless $parameters{namespace};
    die "You must specify a namespaceURI (e.g. http://www.w3.org/2000/svg)" unless $parameters{namespaceURI};

    $parameters{namespacePrefix} = $parameters{namespace} unless $parameters{namespacePrefix};

    close($names);
    die "Failed to read names from file: $namesFile" if (keys %{$hashToFillRef} == 0);
    return $hashToFillRef;
}

sub readAttrs($$)
{
    my ($namesRef, $namesFile) = @_;

    readNames($namesFile, $namesRef, \&attrsHandler);

    $allCppNamespaces{$parameters{namespace}} = { %parameters };
}

sub readElements($$)
{
    my ($namesRef, $namesFile) = @_;

    readNames($namesFile, $namesRef, \&elementsHandler);

    $allCppNamespaces{$parameters{namespace}} = { %parameters };
}

sub elementCount
{
    my $localName = shift;

    return scalar(keys %{$allNamespacesPerElementLocalName{$localName}});
}

sub collectAllElementsAndAttrsPerNamespace
{
    for my $elementKey (keys %allElements) {
        my $namespace = $allElements{$elementKey}{namespace};
        my $localName = $allElements{$elementKey}{localName};
        $allElementsPerNamespace{$namespace} = { } unless exists $allElementsPerNamespace{$namespace};
        $allElementsPerNamespace{$namespace}{$elementKey} = $allElements{$elementKey};
        $allNamespacesPerElementLocalName{$localName} = { } unless exists $allNamespacesPerElementLocalName{$localName};
        $allNamespacesPerElementLocalName{$localName}{$namespace} = 1;
    }
    for my $attrKey (keys %allAttrs) {
        my $namespace = $allAttrs{$attrKey}{namespace};
        $allAttrsPerNamespace{$namespace} = { } unless exists $allAttrsPerNamespace{$namespace};
        $allAttrsPerNamespace{$namespace}{$attrKey} = $allAttrs{$attrKey};
    }
}

sub printMacros
{
    my ($F, $macro, $suffix, $namesRef, $valueField) = @_;

    for my $key (sort keys %$namesRef) {
        my $value = defined $valueField ? $namesRef->{$key}{$valueField} : $key;
        print F "$macro $value$suffix;\n";
    }
}

# Build a direct mapping from the tags to the Element to create.
sub buildConstructorMap
{
    my %tagConstructorMap = ();
    for my $elementKey (keys %allElements) {
        my $interfaceName = $allElements{$elementKey}{interfaceName};

        # Chop the string to keep the interesting part.
        $interfaceName =~ s/$parameters{namespace}(.*)Element/$1/;
        $tagConstructorMap{$elementKey} = lc($interfaceName);
    }

    return %tagConstructorMap;
}

# Helper method that print the constructor's signature avoiding
# unneeded arguments.
sub printConstructorSignature
{
    my ($F, $elementKey, $constructorName, $constructorTagName) = @_;

    print F "static Ref<$parameters{namespace}Element> ${constructorName}Constructor(const QualifiedName& $constructorTagName, Document& document";
    if ($parameters{namespace} eq "HTML") {
        print F ", HTMLFormElement*";
        print F " formElement" if $allElements{$elementKey}{constructorNeedsFormElement};
    }
    print F ", bool";
    print F " createdByParser" if $allElements{$elementKey}{constructorNeedsCreatedByParser};
    print F ")\n{\n";
}

# Helper method to dump the constructor interior and call the 
# Element constructor with the right arguments.
# The variable names should be kept in sync with the previous method.
sub printConstructorInterior
{
    my ($F, $elementKey, $interfaceName, $constructorTagName) = @_;

    # Handle media elements.
    # Note that wrapperOnlyIfMediaIsAvailable is a misnomer, because media availability
    # does not just control the wrapper; it controls the element object that is created.
    # FIXME: Could we instead do this entirely in the wrapper, and use custom wrappers
    # instead of having all the support for this here in this script?
    if ($allElements{$elementKey}{wrapperOnlyIfMediaIsAvailable}) {
        print F <<END
    if (!document.settings().mediaEnabled())
        return $parameters{fallbackInterfaceName}::create($constructorTagName, document);
    
END
;
    }

    my $runtimeCondition;
    my $settingsConditional = $allElements{$elementKey}{settingsConditional};
    my $deprecatedGlobalSettingsConditional = $allElements{$elementKey}{deprecatedGlobalSettingsConditional};
    if ($settingsConditional) {
        $runtimeCondition = "document.settings().${settingsConditional}()";
    } elsif ($deprecatedGlobalSettingsConditional) {
        $runtimeCondition = "DeprecatedGlobalSettings::${deprecatedGlobalSettingsConditional}Enabled()";
    }

    if ($runtimeCondition) {
        print F <<END
    if (!$runtimeCondition)
        return $parameters{fallbackInterfaceName}::create($constructorTagName, document);
END
;
    }

    # Call the constructor with the right parameters.
    print F "    return ${interfaceName}::create($constructorTagName, document";
    print F ", formElement" if $allElements{$elementKey}{constructorNeedsFormElement};
    print F ", createdByParser" if $allElements{$elementKey}{constructorNeedsCreatedByParser};
    print F ");\n}\n";
}

sub printConstructors
{
    my ($F, $tagConstructorMapRef) = @_;
    my %tagConstructorMap = %$tagConstructorMapRef;

    # This is to avoid generating the same constructor several times.
    my %handledInterfaces = ();
    for my $elementKey (sort keys %tagConstructorMap) {
        my $interfaceName = $allElements{$elementKey}{interfaceName};

        # Ignore the mapped tag
        # FIXME: It could be moved inside this loop but was split for readibility.
        next if defined $handledInterfaces{$interfaceName};
        # Elements can have wrappers without constructors.
        # This is useful to make user-agent shadow elements internally testable
        # while keeping them from being avaialble in the HTML markup.
        next if $allElements{$elementKey}{noConstructor};

        $handledInterfaces{$interfaceName} = '1';

        my $conditional = $allElements{$elementKey}{conditional};
        if ($conditional) {
            my $conditionalString = "ENABLE(" . join(") && ENABLE(", split(/&/, $conditional)) . ")";
            print F "#if ${conditionalString}\n";
        }

        printConstructorSignature($F, $elementKey, $tagConstructorMap{$elementKey}, "tagName");
        printConstructorInterior($F, $elementKey, $interfaceName, "tagName");

        if ($conditional) {
            print F "#endif\n";
        }

        print F "\n";
    }
}

sub printFunctionTable
{
    my ($F, $tagConstructorMap) = @_;
    my %tagConstructorMap = %$tagConstructorMap;

    for my $elementKey (sort keys %tagConstructorMap) {
        next if $allElements{$elementKey}{noConstructor};

        my $conditional = $allElements{$elementKey}{conditional};
        if ($conditional) {
            my $conditionalString = "ENABLE(" . join(") && ENABLE(", split(/&/, $conditional)) . ")";
            print F "#if ${conditionalString}\n";
        }

        print F "        { $parameters{namespace}Names::$allElements{$elementKey}{identifier}Tag, $tagConstructorMap{$elementKey}Constructor },\n";

        if ($conditional) {
            print F "#endif\n";
        }
    }
}

sub printTagNameCases
{
    my ($F, $tagConstructorMap) = @_;
    my %tagConstructorMap = %$tagConstructorMap;

    my $argumentList;

    if ($parameters{namespace} eq "HTML") {
        $argumentList = "document, formElement, createdByParser";
    } else {
        $argumentList = "document, createdByParser";
    }

    for my $elementKey (sort keys %tagConstructorMap) {
        next if $allElements{$elementKey}{noConstructor};

        my $conditional = $allElements{$elementKey}{conditional};
        if ($conditional) {
            my $conditionalString = "ENABLE(" . join(") && ENABLE(", split(/&/, $conditional)) . ")";
            print F "#if ${conditionalString}\n";
        }

        print F "    case TagName::$allElements{$elementKey}{tagEnumValue}:\n";
        print F "        return $tagConstructorMap{$elementKey}Constructor($parameters{namespace}Names::$allElements{$elementKey}{identifier}Tag, $argumentList);\n";

        if ($conditional) {
            print F "#endif\n";
        }
    }
}

sub svgCapitalizationHacks
{
    my $name = shift;

    $name = "FE" . ucfirst $1 if $name =~ /^fe(.+)$/;

    return $name;
}

sub upperCaseName
{
    my $name = shift;
    
    $name = svgCapitalizationHacks($name) if ($parameters{namespace} eq "SVG");

    while ($name =~ /^(.*?)_(.*)/) {
        $name = $1 . ucfirst $2;
    }
    
    return ucfirst $name;
}

sub printHeaderHead
{
    my ($F, $prefix, $namespace, $includes, $definitions) = @_;

    print F<<END
#pragma once

$includes

namespace WebCore {

${definitions}namespace ${namespace}Names {

END
    ;
}

sub printCppHead
{
    my ($F, $prefix, $namespace, $includes, $usedNamespace) = @_;

    print F "#include \"config.h\"\n\n";
    print F "#include \"${namespace}Names.h\"\n";
    print F "\n";
    print F $includes;
    print F "\n";
    print F "namespace WebCore {\n";
    print F "\n";
    print F "namespace ${namespace}Names {\n";
    print F "\n";
    print F "using namespace $usedNamespace;\n";
    print F "\n";
}

sub printInit
{
    my ($F, $isDefinition) = @_;

    if ($isDefinition) {
        print F "\nWEBCORE_EXPORT void init();\n\n";
        print F "} }\n\n";
        return;
    }

print F "\nvoid init()
{
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;

    // Use placement new to initialize the globals.
";
}

sub printLicenseHeader
{
    my $F = shift;
    print F "/*
 * THIS FILE WAS AUTOMATICALLY GENERATED, DO NOT EDIT.
 *
 * This file was generated by the dom/make_names.pl script.
 *
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2013 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

";
}

sub printTypeHelpers
{
    my ($F, $namesRef) = @_;

    # Do a first pass to discard classes that map to several tags.
    my %classToKeys = ();
    for my $elementKey (keys %$namesRef) {
        my $class = $allElements{$elementKey}{interfaceName};
        push(@{$classToKeys{$class}}, $elementKey) if defined $class;
    }

    for my $class (sort keys %classToKeys) {
        my $elementKey = $classToKeys{$class}[0];
        next if $allElements{$elementKey}{customTypeHelper};
        # Skip classes that map to more than 1 element.
        my $elementCount = scalar @{$classToKeys{$class}};
        next if $elementCount > 1;

        print F <<END
namespace WebCore {
class $class;
}
namespace WTF {
template<typename ArgType> class TypeCastTraits<const WebCore::$class, ArgType, false /* isBaseType */> {
public:
    static bool isOfType(ArgType& node) { return checkTagName(node); }
private:
END
       ;
       if ($parameters{namespace} eq "HTML" && ($allElements{$elementKey}{wrapperOnlyIfMediaIsAvailable} || $allElements{$elementKey}{settingsConditional} || $allElements{$elementKey}{deprecatedGlobalSettingsConditional})) {
           print F <<END
    static bool checkTagName(const WebCore::HTMLElement& element) { return !element.isHTMLUnknownElement() && element.hasTagName(WebCore::$parameters{namespace}Names::$allElements{$elementKey}{identifier}Tag); }
    static bool checkTagName(const WebCore::Node& node) { return is<WebCore::HTMLElement>(node) && checkTagName(downcast<WebCore::HTMLElement>(node)); }
END
           ;
       } else {
           print F <<END
    static bool checkTagName(const WebCore::$parameters{namespace}Element& element) { return element.hasTagName(WebCore::$parameters{namespace}Names::$allElements{$elementKey}{identifier}Tag); }
    static bool checkTagName(const WebCore::Node& node) { return node.hasTagName(WebCore::$parameters{namespace}Names::$allElements{$elementKey}{identifier}Tag); }
END
           ;
       }
       print F <<END
    static bool checkTagName(const WebCore::EventTarget& target) { return is<WebCore::Node>(target) && checkTagName(downcast<WebCore::Node>(target)); }
};
}
END
       ;
       print F "\n";
    }
}

sub printTypeHelpersHeaderFile
{
    my ($headerPath) = shift;
    my $F;
    open F, ">$headerPath";
    printLicenseHeader($F);

    print F "#pragma once\n\n";
    print F "#include \"".$parameters{namespace}."Names.h\"\n\n";

    # FIXME: Remove `if` condition below once HTMLElementTypeHeaders.h is made inline.
    if ($parameters{namespace} eq "SVG") {
        print F "#include \"".$parameters{namespace}."ElementInlines.h\"\n\n";
    }
    printTypeHelpers($F, \%allElements);

    close F;
}

sub printNamesHeaderFile
{
    my ($headerPath) = shift;
    my $F;
    open F, ">$headerPath";

    printLicenseHeader($F);
    printHeaderHead($F, "DOM", $parameters{namespace}, <<END, "class $parameters{namespace}QualifiedName : public QualifiedName { };\n\n");
#include <wtf/NeverDestroyed.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/text/AtomString.h>
#include "QualifiedName.h"
END

    my $lowercaseNamespacePrefix = lc($parameters{namespacePrefix});

    print F "// Namespace\n";
    print F "WEBCORE_EXPORT extern MainThreadLazyNeverDestroyed<const AtomString> ${lowercaseNamespacePrefix}NamespaceURI;\n\n";

    if (keys %allElements) {
        print F "// Tags\n";
        printMacros($F, "WEBCORE_EXPORT extern LazyNeverDestroyed<const WebCore::$parameters{namespace}QualifiedName>", "Tag", \%allElements, "identifier");
    }

    if (keys %allAttrs) {
        print F "// Attributes\n";
        printMacros($F, "WEBCORE_EXPORT extern LazyNeverDestroyed<const WebCore::QualifiedName>", "Attr", \%allAttrs, "identifier");
    }

    print F "\n";

    if (keys %allElements) {
        print F "const unsigned $parameters{namespace}TagsCount = ", scalar(keys %allElements), ";\n";
        print F "const WebCore::$parameters{namespace}QualifiedName* const* get$parameters{namespace}Tags();\n";
    }

    if (keys %allAttrs) {
        print F "const unsigned $parameters{namespace}AttrsCount = ", scalar(keys %allAttrs), ";\n";
        print F "const WebCore::QualifiedName* const* get$parameters{namespace}Attrs();\n";
    }

    printInit($F, 1);
    close F;
}

sub byElementNameOrder
{
    elementCount($allElements{$a}{localName}) <=> elementCount($allElements{$b}{localName})
        || $allElements{$a}{elementEnumValue} cmp $allElements{$b}{elementEnumValue}
}

sub printTagNameHeaderFile
{
    my ($headerPath) = shift;
    my $F;
    open F, ">$headerPath";

    printLicenseHeader($F);
    print F "#pragma once\n";
    print F "\n";
    print F "#include <wtf/EnumeratedArray.h>\n";
    print F "#include <wtf/NeverDestroyed.h>\n";
    print F "#include <wtf/text/AtomString.h>\n";
    print F "\n";
    print F "namespace WebCore {\n";
    print F "\n";
    print F "class QualifiedName;\n";
    print F "\n";
    print F "enum class TagName : uint16_t {\n";
    print F "    Unknown,\n";

    my %handledTags = ();
    my $previousCppNamespace = "";
    my $seenMultiNamespaceTags = 0;
    for my $elementKey (sort byElementNameOrder keys %allElements) {
        my $identifier = $allElements{$elementKey}{identifier};
        next if $handledTags{$identifier};

        my $localName = $allElements{$elementKey}{localName};
        my $namespace = $allElements{$elementKey}{namespace};
        my $tagEnumValue = $allElements{$elementKey}{tagEnumValue};

        my $sectionComment =
            elementCount($localName) > 1 && !$seenMultiNamespaceTags ? "\n    // Tags in multiple namespaces\n"
            : $allElements{$elementKey}{cppNamespace} ne $previousCppNamespace ? "\n    // $allElements{$elementKey}{cppNamespace}\n"
            : "";
        my $inlineComment = elementCount($localName) > 1 ? " // " . join(", ", sort keys(%{$allNamespacesPerElementLocalName{$localName}})) : "";

        print F $sectionComment;
        print F "    $tagEnumValue,$inlineComment\n";

        $seenMultiNamespaceTags = 1 if elementCount($localName) > 1;
        $previousCppNamespace = $allElements{$elementKey}{cppNamespace};
        $handledTags{$identifier} = 1;
    }

    print F "\n";
    print F "    // Foreign namespace tag names requiring adjustment\n";

    my %namespacesWithTagsRequiringAdjustment = ();
    for my $elementKey (sort byElementNameOrder keys %allElements) {
        next if $allElements{$elementKey}{unadjustedTagEnumValue} eq "";
        print F "    $allElements{$elementKey}{unadjustedTagEnumValue},\n";
        $namespacesWithTagsRequiringAdjustment{$allElements{$elementKey}{namespace}} = 1;
    }

    print F "};\n";
    print F "\n";

    my $lastTagEnumValue;
    for my $elementKey (sort byElementNameOrder keys %allElements) {
        my $unadjustedTagEnumValue = $allElements{$elementKey}{unadjustedTagEnumValue};
        $lastTagEnumValue = $unadjustedTagEnumValue if $unadjustedTagEnumValue ne "";
    }

    print F "inline constexpr auto lastTagNameEnumValue = TagName::$lastTagEnumValue;\n";
    print F "inline LazyNeverDestroyed<EnumeratedArray<TagName, AtomString, lastTagNameEnumValue>> tagNameStrings;\n";
    print F "\n";
    print F "WEBCORE_EXPORT void initializeTagNameStrings();\n";
    print F "TagName findTagName(Span<const UChar>);\n";
    print F "#if ASSERT_ENABLED\n";
    print F "TagName findTagName(const String&);\n";
    print F "#endif\n";
    for my $namespace (sort keys %namespacesWithTagsRequiringAdjustment) {
        print F "TagName adjust${namespace}TagName(TagName);\n";
    }
    print F "\n";
    print F "inline const AtomString& tagNameAsString(TagName tagName)\n";
    print F "{\n";
    print F "    return tagNameStrings.get()[tagName];\n";
    print F "}\n";
    print F "\n";
    for my $namespace (sort keys %namespacesWithTagsRequiringAdjustment) {
        print F "inline TagName adjust${namespace}TagName(TagName tagName)\n";
        print F "{\n";
        print F "    switch (tagName) {\n";
        for my $elementKey (sort byElementNameOrder keys %allElements) {
            next if $allElements{$elementKey}{namespace} ne $namespace || $allElements{$elementKey}{unadjustedTagEnumValue} eq "";
            print F "    case TagName::$allElements{$elementKey}{unadjustedTagEnumValue}:\n";
            print F "        return TagName::$allElements{$elementKey}{tagEnumValue};\n";
        }
        print F "    default:\n";
        print F "        return tagName;\n";
        print F "    }\n";
        print F "}\n";
        print F "\n";
    }
    print F "} // namespace WebCore\n";
    close F;
}

sub printTagNameCppFile
{
    my $cppPath = shift;
    my $F;
    open F, ">$cppPath";

    printLicenseHeader($F);
    print F "#include \"config.h\"\n";
    print F "#include \"TagName.h\"\n";
    print F "\n";
    for my $namespace (sort keys %allCppNamespaces) {
        print F "#include \"${namespace}Names.h\"\n";
    }
    print F "\n";
    print F "namespace WebCore {\n";
    print F "\n";
    print F "static constexpr void* tagQualifiedNamePointers[] = {\n";
    my %handledTags = ();
    for my $elementKey (sort byElementNameOrder keys %allElements) {
        my $cppNamespace = $allElements{$elementKey}{cppNamespace};
        my $identifier = $allElements{$elementKey}{identifier};
        print F "    &${cppNamespace}Names::${identifier}Tag,\n" unless $handledTags{$identifier};
        $handledTags{$identifier} = 1;
    }
    print F "};\n";
    print F "\n";
    print F "static constexpr StringImpl::StaticStringImpl unadjustedTagNames[] = {\n";
    for my $elementKey (sort byElementNameOrder keys %allElements) {
        next if $allElements{$elementKey}{unadjustedTagEnumValue} eq "";
        print F "    StringImpl::StaticStringImpl { \"$allElements{$elementKey}{parsedTagName}\" },\n";
    }
    print F "};\n";
    print F "\n";
    print F "void initializeTagNameStrings() {\n";
    print F "    static bool initialized = false;\n";
    print F "    if (std::exchange(initialized, true))\n";
    print F "        return;\n";
    print F "\n";
    print F "    tagNameStrings.construct();\n";
    print F "    auto tagNamesEntry = tagNameStrings->begin();\n";
    print F "    ++tagNamesEntry; // Skip TagName::Unknown\n";
    print F "    for (auto* qualifiedName : tagQualifiedNamePointers)\n";
    print F "        *(tagNamesEntry++) = reinterpret_cast<LazyNeverDestroyed<QualifiedName>*>(qualifiedName)->get().localName();\n";
    print F "    for (auto& string : unadjustedTagNames) {\n";
    print F "        reinterpret_cast<const StringImpl&>(string).assertHashIsCorrect();\n";
    print F "        *(tagNamesEntry++) = AtomString(&string);\n";
    print F "    }\n";
    print F "    ASSERT(tagNamesEntry == tagNameStrings->end());\n";
    print F "}\n";
    print F "\n";
    print F "template <typename characterType>\n";
    print F "static inline TagName findTagFromBuffer(Span<const characterType> buffer)\n";
    print F "{\n";
    generateFindBody(\%allElements, \&byElementNameOrder, "parsedTagName", "TagName", "parsedTagEnumValue");
    print F "}\n";
    print F "\n";
    print F "TagName findTagName(Span<const UChar> buffer)\n";
    print F "{\n";
    print F "    return findTagFromBuffer(buffer);\n";
    print F "}\n";
    print F "\n";
    print F "#if ASSERT_ENABLED\n";
    print F "TagName findTagName(const String& name)\n";
    print F "{\n";
    print F "    if (name.is8Bit())\n";
    print F "        return findTagFromBuffer(Span(name.characters8(), name.length()));\n";
    print F "    return findTagFromBuffer(Span(name.characters16(), name.length()));\n";
    print F "}\n";
    print F "#endif\n";
    print F "\n";
    print F "} // namespace WebCore\n";
    close F;
}

sub printElementNameHeaderFile
{
    my ($headerPath) = shift;
    my $F;
    open F, ">$headerPath";

    printLicenseHeader($F);
    print F "#pragma once\n";
    print F "\n";
    print F "#include \"Namespace.h\"\n";
    print F "#include \"TagName.h\"\n";
    print F "#include <wtf/Forward.h>\n";
    print F "\n";
    print F "namespace WebCore {\n";
    print F "\n";
    print F "class QualifiedName;\n";
    print F "\n";
    print F "enum class ElementName : uint16_t {\n";
    print F "    Unknown,\n";
    for my $elementKey (sort byElementNameOrder keys %allElements) {
        print F "    $allElements{$elementKey}{elementEnumValue},\n";
    }
    print F "};\n";
    print F "\n";
    print F "namespace ElementNames {\n";
    for my $namespace (sort keys %allElementsPerNamespace) {
        print F "namespace $namespace {\n";
        for my $elementKey (sort byElementNameOrder keys %{$allElementsPerNamespace{$namespace}}) {
            print F "inline constexpr auto $allElements{$elementKey}{tagEnumValue} = ElementName::$allElements{$elementKey}{elementEnumValue};\n";
        }
        print F "} // namespace $namespace\n";
    }
    print F "} // namespace ElementNames\n";
    print F "\n";
    print F "ElementName findElementName(Namespace, const String&);\n";
    print F "TagName tagNameForElement(ElementName);\n";
    print F "ElementName elementNameForTag(Namespace, TagName);\n";
    print F "const QualifiedName& qualifiedNameForElement(ElementName);\n";
    print F "\n";

    my $lastUniqueTagEnumValue;
    my %firstUniqueTagEnumValueByNamespace = ();
    my %lastUniqueTagEnumValueByNamespace = ();
    for my $elementKey (sort byElementNameOrder keys %allElements) {
        my $localName = $allElements{$elementKey}{localName};
        my $namespace = $allElements{$elementKey}{namespace};
        my $tagEnumValue = $allElements{$elementKey}{tagEnumValue};
        if (elementCount($localName) == 1) {
            $lastUniqueTagEnumValue = $tagEnumValue;
            $firstUniqueTagEnumValueByNamespace{$namespace} = $tagEnumValue unless exists $firstUniqueTagEnumValueByNamespace{$namespace};
            $lastUniqueTagEnumValueByNamespace{$namespace} = $tagEnumValue;
        }
    }

    print F "inline TagName tagNameForElement(ElementName elementName)\n";
    print F "{\n";
    print F "    constexpr auto s_lastUniqueTagName = TagName::$lastUniqueTagEnumValue;\n";
    print F"\n";
    print F "    if (LIKELY(static_cast<uint16_t>(elementName) <= static_cast<uint16_t>(s_lastUniqueTagName)))\n";
    print F "        return static_cast<TagName>(elementName);\n";
    print F "\n";
    print F "    switch (elementName) {\n";
    for my $elementKey (sort byElementNameOrder keys %allElements) {
        next if elementCount($allElements{$elementKey}{localName}) == 1;
        print F "    case ElementName::$allElements{$elementKey}{elementEnumValue}:\n";
        print F "        return TagName::$allElements{$elementKey}{tagEnumValue};\n";
    }
    print F "    default:\n";
    print F "        break;\n";
    print F "    }\n";
    print F "\n";
    print F "    return TagName::Unknown;\n";
    print F "}\n";
    print F "\n";
    print F "inline ElementName elementNameForTag(Namespace ns, TagName tagName)\n";
    print F "{\n";
    for my $namespace (sort keys %allCppNamespaces) {
        next unless grep { $allElements{$_}{cppNamespace} eq $namespace } keys %allElements;
        my $namespaceCondition = "ns == Namespace::$namespace";
        print F "    if ($namespaceCondition) {\n";
        print F "        constexpr auto s_firstUnique${namespace}TagName = TagName::$firstUniqueTagEnumValueByNamespace{$namespace};\n";
        print F "        constexpr auto s_lastUnique${namespace}TagName = TagName::$lastUniqueTagEnumValueByNamespace{$namespace};\n";
        print F "\n";
        print F "        if (UNLIKELY(static_cast<uint16_t>(tagName) < static_cast<uint16_t>(s_firstUnique${namespace}TagName)))\n";
        print F "            return ElementName::Unknown;\n";
        print F "\n";
        print F "        if (LIKELY(static_cast<uint16_t>(tagName) <= static_cast<uint16_t>(s_lastUnique${namespace}TagName)))\n";
        print F "            return static_cast<ElementName>(tagName);\n";
        print F "\n";
        my @tagKeysForNonUniqueTags = grep { elementCount($allElements{$_}{localName}) > 1 && $allElements{$_}{namespace} eq $namespace } sort byElementNameOrder keys %allElements;
        if (@tagKeysForNonUniqueTags) {
            print F "        switch (tagName) {\n";
            for my $elementKey (@tagKeysForNonUniqueTags) {
                print F "        case TagName::$allElements{$elementKey}{tagEnumValue}:\n";
                print F "            return ElementName::$allElements{$elementKey}{elementEnumValue};\n";
            }
            print F "        default:\n";
            print F "            break;\n";
            print F "        }\n";
        }
        print F "        return ElementName::Unknown;\n";
        print F "    }\n";
        print F "\n";
    }
    print F "    return ElementName::Unknown;\n";
    print F "}\n";
    print F "\n";
    print F "} // namespace WebCore\n";
    close F;
}

sub printElementNameCppFile
{
    my $cppPath = shift;
    my $F;
    open F, ">$cppPath";

    printLicenseHeader($F);
    print F "#include \"config.h\"\n";
    print F "#include \"ElementName.h\"\n";
    print F "\n";
    for my $namespace (sort keys %allCppNamespaces) {
        print F "#include \"${namespace}Names.h\"\n";
    }
    print F "\n";
    print F "namespace WebCore {\n";
    print F "\n";
    for my $namespace (sort keys %allElementsPerNamespace) {
        my $namespaceIdentifier = $namespace eq "" ? "NoNamespace" : $namespace;
        print F "template <typename characterType>\n";
        print F "static inline ElementName find${namespaceIdentifier}Element(Span<const characterType> buffer)\n";
        print F "{\n";
        generateFindBody($allElementsPerNamespace{$namespace}, \&byElementNameOrder, "localName", "ElementName", "elementEnumValue");
        print F "}\n";
        print F "\n";
    }
    print F "template <typename characterType>\n";
    print F "static inline ElementName findElementFromBuffer(Namespace ns, Span<const characterType> buffer)\n";
    print F "{\n";
    print F "    switch (ns) {\n";
    for my $namespace (sort keys %allElementsPerNamespace) {
        my $namespaceEnumValue = $namespace eq "" ? "None" : $namespace;
        my $namespaceIdentifier = $namespace eq "" ? "NoNamespace" : $namespace;
        print F "    case Namespace::$namespaceEnumValue:\n";
        print F "        return find${namespaceIdentifier}Element(buffer);\n";
    }
    print F "    default:\n";
    print F "        return ElementName::Unknown;\n";
    print F "    }\n";
    print F "}\n";
    print F "\n";
    print F "ElementName findElementName(Namespace ns, const String& name)\n";
    print F "{\n";
    print F "    if (name.is8Bit())\n";
    print F "        return findElementFromBuffer(ns, Span(name.characters8(), name.length()));\n";
    print F "    return findElementFromBuffer(ns, Span(name.characters16(), name.length()));\n";
    print F "}\n";
    print F "\n";
    print F "const QualifiedName& qualifiedNameForElement(ElementName elementName)\n";
    print F "{\n";
    print F "    ASSERT(elementName != ElementName::Unknown);\n";
    print F "    switch (elementName) {\n";
    print F "    case ElementName::Unknown:\n";
    print F "        break;\n";
    for my $elementKey (sort byElementNameOrder keys %allElements) {
        print F "    case ElementName::$allElements{$elementKey}{elementEnumValue}:\n";
        print F "        return $allElements{$elementKey}{cppNamespace}Names::$allElements{$elementKey}{identifier}Tag;\n";
    }
    print F "    }\n";
    print F "    return nullQName();\n";
    print F "}\n";
    print F "\n";
    print F "} // namespace WebCore\n";
    close F;
}

sub printNamespaceHeaderFile
{
    my ($headerPath) = shift;
    my $F;
    open F, ">$headerPath";

    printLicenseHeader($F);
    print F "#pragma once\n";
    print F "\n";
    print F "#include <wtf/Forward.h>\n";
    print F "\n";
    print F "namespace WebCore {\n";
    print F "\n";
    print F "enum class Namespace : uint8_t {\n";
    print F "    Unknown,\n";
    print F "    None,\n";
    for my $namespace (sort keys %allCppNamespaces) {
        print F "    $namespace,\n";
    }
    print F "};\n";
    print F "\n";
    print F "Namespace findNamespace(const AtomString&);\n";
    print F "\n";
    print F "} // namespace WebCore\n";
    close F;
}

sub findMaxStringLength
{
    my $candidates = shift;

    my $maxLength = 0;
    for my $candidate (@$candidates) {
        $maxLength = length($candidate->{string}) if length($candidate->{string}) > $maxLength;
    }

    return $maxLength;
}

sub candidatesWithStringLength
{
    my $candidates = shift;
    my $expectedLength = shift;

    return grep { length($_->{string}) == $expectedLength } @$candidates;
}

sub generateFindNameForLength
{
    my $indent = shift;
    my $candidates = shift;
    my $length = shift;
    my $currentIndex = shift;
    my $enumClass = shift;

    my $candidateCount = @$candidates;
    if ($candidateCount == 1) {
        my $candidate = $candidates->[0];
        my $string = $candidate->{string};
        my $enumValue = $candidate->{enumValue};
        my $needsIfCheck = $currentIndex < $length;
        if ($needsIfCheck) {
            my $lengthToCompare = $length - $currentIndex;
            if ($lengthToCompare == 1) {
                my $letter = substr($string, $currentIndex, 1);
                print F "${indent}if (buffer[$currentIndex] == '$letter') {\n";
            } else {
                my $bufferStart = $currentIndex > 0 ? "buffer.data() + $currentIndex" : "buffer.data()";
                print F "${indent}static constexpr characterType rest[] = { ";
                for (my $index = $currentIndex; $index < $length; $index = $index + 1) {
                    my $letter = substr($string, $index, 1);
                    print F "'$letter', ";
                }
                print F "};\n";
                print F "${indent}if (WTF::equal($bufferStart, rest, $lengthToCompare)) {\n";
            }
            print F "$indent    return ${enumClass}::$enumValue;\n";
            print F "$indent}\n";
        } else {
            print F "${indent}return ${enumClass}::$enumValue;\n";
        }
        return;
    }
    for (my $i = 0; $i < $candidateCount;) {
        my $candidate = $candidates->[$i];
        my $string = $candidate->{string};
        my $enumValue = $candidate->{enumValue};
        my $letterAtIndex = substr($string, $currentIndex, 1);
        print F "${indent}if (buffer[$currentIndex] == '$letterAtIndex') {\n";
        my @candidatesWithPrefix = ($candidate);
        for ($i = $i + 1; $i < $candidateCount; $i = $i + 1) {
            my $nextCandidate = $candidates->[$i];
            my $nextString = $nextCandidate->{string};
            if (substr($nextString, $currentIndex, 1) eq $letterAtIndex) {
                push(@candidatesWithPrefix, $nextCandidate);
            } else {
                last;
            }
        }
        generateFindNameForLength($indent . "    ", \@candidatesWithPrefix, $length, $currentIndex + 1, $enumClass);
        if (@candidatesWithPrefix > 1) {
            print F "${indent}    return ${enumClass}::Unknown;\n";
        }
        print F "$indent}\n";
    }
}

sub generateFindBody {
    my $names = shift;
    my $keySort = shift;
    my $field = shift;
    my $enumClass = shift;
    my $enumField = shift;

    my @candidates = ();
    my %handledStrings = ();
    for my $key (sort $keySort keys %$names) {
        my $string = $names->{$key}{$field};
        my $enumValue = $names->{$key}{$enumField};
        push @candidates, { key => $key, string => $string, enumValue => $enumValue } unless defined $string && $handledStrings{$string};
        $handledStrings{$string} = 1;
    }

    my $maxStringLength = findMaxStringLength(\@candidates);
    print F "    switch (buffer.size()) {\n";
    for (my $length = 1; $length <= $maxStringLength; $length = $length + 1) {
        my @candidatesForLength = sort { $a->{string} cmp $b->{string} } candidatesWithStringLength(\@candidates, $length);
        next unless @candidatesForLength;
        print F "    case $length: {\n";
        generateFindNameForLength("        ", \@candidatesForLength, $length, 0, $enumClass);
        print F "        break;\n";
        print F "    }\n";
    }
    print F "    default:\n";
    print F "        break;\n";
    print F "    };\n";
    print F "    return ${enumClass}::Unknown;\n";
}

sub printNamesCppFile
{
    my $cppPath = shift;
    my $F;
    open F, ">$cppPath";
    
    printLicenseHeader($F);
    printCppHead($F, "DOM", $parameters{namespace}, <<END, "WebCore");
#include "ElementName.h"
#include "Namespace.h"
END
    
    my $lowercaseNamespacePrefix = lc($parameters{namespacePrefix});

    print F "MainThreadLazyNeverDestroyed<const AtomString> ${lowercaseNamespacePrefix}NamespaceURI;\n\n";

    my %allStrings = ();
    for my $elementKey (keys %allElements) {
        $allStrings{$allElements{$elementKey}{identifier}} = $allElements{$elementKey}{localName};
    }
    for my $attrKey (keys %allAttrs) {
        $allStrings{$allAttrs{$attrKey}{identifier}} = $allAttrs{$attrKey}{localName};
    }

    print F StaticString::GenerateStrings(\%allStrings);

    if (keys %allElements) {
        print F "// Tags\n";
        for my $elementKey (sort keys %allElements) {
            print F "WEBCORE_EXPORT LazyNeverDestroyed<const $parameters{namespace}QualifiedName> $allElements{$elementKey}{identifier}Tag;\n";
        }

        print F "\n\nconst WebCore::$parameters{namespace}QualifiedName* const* get$parameters{namespace}Tags()\n";
        print F "{\n    static const WebCore::$parameters{namespace}QualifiedName* const $parameters{namespace}Tags[] = {\n";
        for my $elementKey (sort keys %allElements) {
            print F "        &$allElements{$elementKey}{identifier}Tag.get(),\n";
        }
        print F "    };\n";
        print F "    return $parameters{namespace}Tags;\n";
        print F "}\n";
    }

    if (keys %allAttrs) {
        print F "\n// Attributes\n";
        for my $attrKey (sort keys %allAttrs) {
            print F "WEBCORE_EXPORT LazyNeverDestroyed<const QualifiedName> $allAttrs{$attrKey}{identifier}Attr;\n";
        }
        print F "\n\nconst WebCore::QualifiedName* const* get$parameters{namespace}Attrs()\n";
        print F "{\n    static const WebCore::QualifiedName* const $parameters{namespace}Attrs[] = {\n";
        for my $attrKey (sort keys %allAttrs) {
            print F "        &$allAttrs{$attrKey}{identifier}Attr.get(),\n";
        }
        print F "    };\n";
        print F "    return $parameters{namespace}Attrs;\n";
        print F "}\n";
    }

    printInit($F, 0);

    print(F "    AtomString ${lowercaseNamespacePrefix}NS(\"$parameters{namespaceURI}\"_s);\n\n");

    print(F "    // Namespace\n");
    print(F "    ${lowercaseNamespacePrefix}NamespaceURI.construct(${lowercaseNamespacePrefix}NS);\n");
    print(F "\n");
    print F StaticString::GenerateStringAsserts(\%allStrings);

    if (keys %allElements) {
        my $tagsNamespace = $parameters{elementsNullNamespace} ? "nullAtom()" : "${lowercaseNamespacePrefix}NS";
        my $tagsNamespaceEnumValue = $parameters{elementsNullNamespace} ? "None" : $parameters{namespace};
        printDefinitions($F, \%allElements, "tags", $tagsNamespace, $tagsNamespaceEnumValue);
    }
    if (keys %allAttrs) {
        my $attrsNamespace = $parameters{attrsNullNamespace} ? "nullAtom()" : "${lowercaseNamespacePrefix}NS";
        my $attrsNamespaceEnumValue = $parameters{attrsNullNamespace} ? "None" : $parameters{namespace};
        printDefinitions($F, \%allAttrs, "attributes", $attrsNamespace, $attrsNamespaceEnumValue);
    }

    print F "}\n\n} }\n\n";
    close F;
}

sub printNamespaceCppFile
{
    my $cppPath = shift;
    my $F;
    open F, ">$cppPath";

    printLicenseHeader($F);
    print F "#include \"config.h\"\n";
    print F "#include \"Namespace.h\"\n";
    print F "\n";
    print F "#include <wtf/text/AtomString.h>\n";
    print F "\n";
    for my $namespace (sort keys %allCppNamespaces) {
        print F "#include \"${namespace}Names.h\"\n";
    }
    print F "\n";
    print F "namespace WebCore {\n";
    print F "\n";
    print F "Namespace findNamespace(const AtomString& namespaceURI)\n";
    print F "{\n";
    print F "    if (namespaceURI.isEmpty())\n";
    print F "        return Namespace::None;\n";
    for my $namespace (sort keys %allCppNamespaces) {
        my $lowercaseNamespacePrefix = lc $allCppNamespaces{$namespace}{namespacePrefix};
        print F "    if (namespaceURI == ${namespace}Names::${lowercaseNamespacePrefix}NamespaceURI)\n";
        print F "        return Namespace::${namespace};\n";
    }
    print F "    return Namespace::Unknown;\n";
    print F "}\n";
    print F "\n";
    print F "} // namespace WebCore\n";
    close F;
}

sub printJSElementIncludes
{
    my $F = shift;

    my %tagsSeen;
    for my $elementKey (sort keys %allElements) {
        my $JSInterfaceName = $allElements{$elementKey}{JSInterfaceName};
        next if $tagsSeen{$JSInterfaceName} || usesDefaultJSWrapper($elementKey);
        if ($allElements{$elementKey}{conditional}) {
            # We skip feature-define-specific #includes here since we handle them separately.
            next;
        }
        $tagsSeen{$JSInterfaceName} = 1;

        print F "#include \"JS${JSInterfaceName}.h\"\n";
    }
    print F "#include \"JS$parameters{fallbackJSInterfaceName}.h\"\n";
}

sub printElementIncludes
{
    my $F = shift;

    my %tagsSeen;
    for my $elementKey (sort keys %allElements) {
        my $interfaceName = $allElements{$elementKey}{interfaceName};
        next if $tagsSeen{$interfaceName};
        if ($allElements{$elementKey}{conditional}) {
            # We skip feature-define-specific #includes here since we handle them separately.
            next;
        }
        $tagsSeen{$interfaceName} = 1;

        print F "#include \"${interfaceName}.h\"\n";
    }
    print F "#include \"$parameters{fallbackInterfaceName}.h\"\n";
}

sub printConditionalElementIncludes
{
    my ($F, $wrapperIncludes) = @_;

    my %conditionals;
    my %unconditionalElementIncludes;
    my %unconditionalJSElementIncludes;

    for my $elementKey (keys %allElements) {
        my $conditional = $allElements{$elementKey}{conditional};
        my $interfaceName = $allElements{$elementKey}{interfaceName};
        my $JSInterfaceName = $allElements{$elementKey}{JSInterfaceName};

        if ($conditional) {
            $conditionals{$conditional}{interfaceNames}{$interfaceName} = 1;
            $conditionals{$conditional}{JSInterfaceNames}{$JSInterfaceName} = 1;
        } else {
            $unconditionalElementIncludes{$interfaceName} = 1;
            $unconditionalJSElementIncludes{$JSInterfaceName} = 1;
        }
    }

    for my $conditional (sort keys %conditionals) {
        print F "\n#if ENABLE($conditional)\n";
        for my $interfaceName (sort keys %{$conditionals{$conditional}{interfaceNames}}) {
            next if $unconditionalElementIncludes{$interfaceName};
            print F "#include \"$interfaceName.h\"\n";
        }
        if ($wrapperIncludes) {
            for my $JSInterfaceName (sort keys %{$conditionals{$conditional}{JSInterfaceNames}}) {
                next if $unconditionalJSElementIncludes{$JSInterfaceName};
                print F "#include \"JS$JSInterfaceName.h\"\n";
            }
        }
        print F "#endif\n";
    }
}

sub printDefinitions
{
    my ($F, $namesRef, $type, $namespaceURI, $namespaceEnumValue) = @_;

    my $shortCamelType = ucfirst(substr(substr($type, 0, -1), 0, 4));
    my $capitalizedType = ucfirst($type);

    my @tableEntryFields = (
        "LazyNeverDestroyed<const QualifiedName>* targetAddress",
        "const StaticStringImpl& name",
        "ElementName elementName"
    );

    my $cast = $type eq "tags" ? "(LazyNeverDestroyed<const QualifiedName>*)" : "";

    print F "\n";
    print F "    struct ${capitalizedType}TableEntry {\n";

    print F map { "        $_;\n" } @tableEntryFields;

    print F "    };\n";
    print F "\n";
    print F "    static const ${capitalizedType}TableEntry ${type}Table[] = {\n";

    for my $key (sort keys %$namesRef) {
        my $identifier = $namesRef->{$key}{identifier};
        my $elementEnumValue = $namesRef->{$key}{elementEnumValue} || "Unknown";
        # Attribute names never correspond to a recognized ElementName.
        print F "        { $cast&$identifier$shortCamelType, *(&${identifier}Data), ElementName::$elementEnumValue },\n";
    }

    print F "    };\n";
    print F "\n";
    print F "    for (auto& entry : ${type}Table)\n";
    print F "        entry.targetAddress->construct(nullAtom(), AtomString(&entry.name), $namespaceURI, Namespace::$namespaceEnumValue, entry.elementName);\n";
}

## ElementFactory routines

sub printFactoryCppFile
{
    my $cppPath = shift;
    my $F;
    open F, ">$cppPath";

    my $formElementArgumentForDeclaration = "";
    my $formElementArgumentForDefinition = "";
    $formElementArgumentForDeclaration = ", HTMLFormElement*" if $parameters{namespace} eq "HTML";
    $formElementArgumentForDefinition = ", HTMLFormElement* formElement" if $parameters{namespace} eq "HTML";

    printLicenseHeader($F);

    print F <<END
#include "config.h"
END
    ;

    print F "\n#if $parameters{guardFactoryWith}\n\n" if $parameters{guardFactoryWith};

    print F <<END
#include "$parameters{namespace}ElementFactory.h"

#include "$parameters{namespace}Names.h"

END
    ;

    printElementIncludes($F);
    printConditionalElementIncludes($F, 0);

    print F <<END

#include "DeprecatedGlobalSettings.h"
#include "Document.h"
#include "Settings.h"
#include "TagName.h"
#include <wtf/RobinHoodHashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

using $parameters{namespace}ConstructorFunction = Ref<$parameters{namespace}Element> (*)(const QualifiedName&, Document&$formElementArgumentForDeclaration, bool createdByParser);

END
    ;

    my %tagConstructorMap = buildConstructorMap();
    my $argumentList;

    if ($parameters{namespace} eq "HTML") {
        $argumentList = "name, document, formElement, createdByParser";
    } else {
        $argumentList = "name, document, createdByParser";
    }

    my $lowercaseNamespacePrefix = lc($parameters{namespacePrefix});

    printConstructors($F, \%tagConstructorMap);

    my $firstTagIdentifier;
    for my $elementKey (sort keys %tagConstructorMap) {
        $firstTagIdentifier = $allElements{$elementKey}{identifier};
        last;
    }

    print F <<END

struct $parameters{namespace}ConstructorFunctionMapEntry {
    $parameters{namespace}ConstructorFunction function { nullptr };
    const QualifiedName* qualifiedName { nullptr }; // Use pointer instead of reference so that emptyValue() in HashMap is cheap to create.
};

static NEVER_INLINE MemoryCompactLookupOnlyRobinHoodHashMap<AtomString, $parameters{namespace}ConstructorFunctionMapEntry> create$parameters{namespace}FactoryMap()
{
    struct TableEntry {
        decltype($parameters{namespace}Names::${firstTagIdentifier}Tag)& name;
        $parameters{namespace}ConstructorFunction function;
    };

    static constexpr TableEntry table[] = {
END
    ;

    printFunctionTable($F, \%tagConstructorMap);

    print F <<END
    };

    MemoryCompactLookupOnlyRobinHoodHashMap<AtomString, $parameters{namespace}ConstructorFunctionMapEntry> map;
    for (auto& entry : table)
        map.add(entry.name.get().localName(), $parameters{namespace}ConstructorFunctionMapEntry { entry.function, &entry.name.get() });
    return map;
}

static $parameters{namespace}ConstructorFunctionMapEntry find$parameters{namespace}ElementConstructorFunction(const AtomString& localName)
{
    static NeverDestroyed map = create$parameters{namespace}FactoryMap();
    return map.get().get(localName);
}

RefPtr<$parameters{namespace}Element> $parameters{namespace}ElementFactory::createKnownElement(const AtomString& localName, Document& document$formElementArgumentForDefinition, bool createdByParser)
{
    const $parameters{namespace}ConstructorFunctionMapEntry& entry = find$parameters{namespace}ElementConstructorFunction(localName);
    if (LIKELY(entry.function)) {
        ASSERT(entry.qualifiedName);
        const auto& name = *entry.qualifiedName;
        return entry.function($argumentList);
    }
    return nullptr;
}

RefPtr<$parameters{namespace}Element> $parameters{namespace}ElementFactory::createKnownElement(const QualifiedName& name, Document& document$formElementArgumentForDefinition, bool createdByParser)
{
    const $parameters{namespace}ConstructorFunctionMapEntry& entry = find$parameters{namespace}ElementConstructorFunction(name.localName());
    if (LIKELY(entry.function))
        return entry.function($argumentList);
    return nullptr;
}

RefPtr<$parameters{namespace}Element> $parameters{namespace}ElementFactory::createKnownElement(TagName tagName, Document& document$formElementArgumentForDefinition, bool createdByParser)
{
    switch (tagName) {
END
    ;

    printTagNameCases($F, \%tagConstructorMap);

    print F <<END
    default:
        return nullptr;
    }
}

Ref<$parameters{namespace}Element> $parameters{namespace}ElementFactory::createElement(const AtomString& localName, Document& document$formElementArgumentForDefinition, bool createdByParser)
{
    const $parameters{namespace}ConstructorFunctionMapEntry& entry = find$parameters{namespace}ElementConstructorFunction(localName);
    if (LIKELY(entry.function)) {
        ASSERT(entry.qualifiedName);
        const auto& name = *entry.qualifiedName;
        return entry.function($argumentList);
    }
    return $parameters{fallbackInterfaceName}::create(QualifiedName(nullAtom(), localName, $parameters{namespace}Names::${lowercaseNamespacePrefix}NamespaceURI), document);
}

Ref<$parameters{namespace}Element> $parameters{namespace}ElementFactory::createElement(const QualifiedName& name, Document& document$formElementArgumentForDefinition, bool createdByParser)
{
    const $parameters{namespace}ConstructorFunctionMapEntry& entry = find$parameters{namespace}ElementConstructorFunction(name.localName());
    if (LIKELY(entry.function))
        return entry.function($argumentList);
    return $parameters{fallbackInterfaceName}::create(name, document);
}

} // namespace WebCore

END
    ;

    print F "#endif\n" if $parameters{guardFactoryWith};

    close F;
}

sub printFactoryHeaderFile
{
    my $headerPath = shift;
    my $F;
    open F, ">$headerPath";

    printLicenseHeader($F);

    print F<<END
#pragma once

#include <wtf/Forward.h>

namespace WebCore {

class Document;
class HTMLFormElement;
class QualifiedName;

class $parameters{namespace}Element;

enum class TagName : uint16_t;

class $parameters{namespace}ElementFactory {
public:
END
;

print F "    static RefPtr<$parameters{namespace}Element> createKnownElement(const AtomString&, Document&";
print F ", HTMLFormElement* = nullptr" if $parameters{namespace} eq "HTML";
print F ", bool createdByParser = false);\n";

print F "    static RefPtr<$parameters{namespace}Element> createKnownElement(const QualifiedName&, Document&";
print F ", HTMLFormElement* = nullptr" if $parameters{namespace} eq "HTML";
print F ", bool createdByParser = false);\n";

print F "    static RefPtr<$parameters{namespace}Element> createKnownElement(TagName, Document&";
print F ", HTMLFormElement* = nullptr" if $parameters{namespace} eq "HTML";
print F ", bool createdByParser = false);\n";

print F "    static Ref<$parameters{namespace}Element> createElement(const AtomString&, Document&";
print F ", HTMLFormElement* = nullptr" if $parameters{namespace} eq "HTML";
print F ", bool createdByParser = false);\n";

print F "    static Ref<$parameters{namespace}Element> createElement(const QualifiedName&, Document&";
print F ", HTMLFormElement* = nullptr" if $parameters{namespace} eq "HTML";
print F ", bool createdByParser = false);\n";

printf F <<END
};

}

END
;

    close F;
}

## Wrapper Factory routines

sub usesDefaultJSWrapper
{
    my $elementKey = shift;

    # An element reuses the default wrapper if its JSInterfaceName matches the default namespace Element.
    return $allElements{$elementKey}{JSInterfaceName} eq $parameters{namespace} . "Element";
}

sub printWrapperFunctions
{
    my $F = shift;

    my %tagsSeen;
    for my $elementKey (sort keys %allElements) {
        # Avoid defining the same wrapper method twice.
        my $JSInterfaceName = $allElements{$elementKey}{JSInterfaceName};
        next if ($tagsSeen{$JSInterfaceName} || (usesDefaultJSWrapper($elementKey) && ($parameters{fallbackJSInterfaceName} eq $parameters{namespace} . "Element"))) && !$allElements{$elementKey}{settingsConditional};
        $tagsSeen{$JSInterfaceName} = 1;

        my $conditional = $allElements{$elementKey}{conditional};
        if ($conditional) {
            my $conditionalString = "ENABLE(" . join(") && ENABLE(", split(/&/, $conditional)) . ")";
            print F "#if ${conditionalString}\n\n";
        }

        if ($allElements{$elementKey}{wrapperOnlyIfMediaIsAvailable}) {
            print F <<END
static JSDOMObject* create${JSInterfaceName}Wrapper(JSDOMGlobalObject* globalObject, Ref<$parameters{namespace}Element>&& element)
{
    if (element->is$parameters{fallbackInterfaceName}())
        return createWrapper<$parameters{fallbackInterfaceName}>(globalObject, WTFMove(element));
    return createWrapper<${JSInterfaceName}>(globalObject, WTFMove(element));
}

END
            ;
        } elsif ($allElements{$elementKey}{settingsConditional}) {
            print F <<END
static JSDOMObject* create$allElements{$elementKey}{interfaceName}Wrapper(JSDOMGlobalObject* globalObject, Ref<$parameters{namespace}Element>&& element)
{
    if (element->is$parameters{fallbackInterfaceName}())
        return createWrapper<$parameters{fallbackInterfaceName}>(globalObject, WTFMove(element));
    return createWrapper<${JSInterfaceName}>(globalObject, WTFMove(element));
}

END
            ;
        } elsif ($allElements{$elementKey}{deprecatedGlobalSettingsConditional}) {
            my $deprecatedGlobalSettingsConditional = $allElements{$elementKey}{deprecatedGlobalSettingsConditional};
            print F <<END
static JSDOMObject* create${JSInterfaceName}Wrapper(JSDOMGlobalObject* globalObject, Ref<$parameters{namespace}Element>&& element)
{
    if (element->is$parameters{fallbackInterfaceName}())
        return createWrapper<$parameters{fallbackJSInterfaceName}>(globalObject, WTFMove(element));
    return createWrapper<${JSInterfaceName}>(globalObject, WTFMove(element));
}
END
    ;
        } else {
            print F <<END
static JSDOMObject* create${JSInterfaceName}Wrapper(JSDOMGlobalObject* globalObject, Ref<$parameters{namespace}Element>&& element)
{
    return createWrapper<${JSInterfaceName}>(globalObject, WTFMove(element));
}

END
    ;
        }

        if ($conditional) {
            print F "#endif\n\n";
        }
    }
}

sub printWrapperFactoryCppFile
{
    my $outputDir = shift;
    my $wrapperFactoryFileName = shift;
    my $F;
    open F, ">" . $outputDir . "/JS" . $wrapperFactoryFileName . ".cpp";

    printLicenseHeader($F);

    print F "#include \"config.h\"\n";
    print F "#include \"JS$parameters{namespace}ElementWrapperFactory.h\"\n\n";

    print F "\n#if $parameters{guardFactoryWith}\n\n" if $parameters{guardFactoryWith};

    printJSElementIncludes($F);
    printElementIncludes($F);

    print F "\n#include \"$parameters{namespace}Names.h\"\n";
    print F <<END

#include "DeprecatedGlobalSettings.h"
#include "Document.h"
#include "Settings.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/StdLibExtras.h>
END
;

    printConditionalElementIncludes($F, 1);

    print F <<END

using namespace JSC;

namespace WebCore {

using Create$parameters{namespace}ElementWrapperFunction = JSDOMObject* (*)(JSDOMGlobalObject*, Ref<$parameters{namespace}Element>&&);

END
;

    printWrapperFunctions($F);

    my $firstTagIdentifier;
    for my $elementKey (sort keys %allElements) {
        $firstTagIdentifier = $allElements{$elementKey}{identifier};
        last;
    }

print F <<END

static NEVER_INLINE MemoryCompactLookupOnlyRobinHoodHashMap<AtomString, Create$parameters{namespace}ElementWrapperFunction> create$parameters{namespace}WrapperMap()
{
    struct TableEntry {
        decltype($parameters{namespace}Names::${firstTagIdentifier}Tag)& name;
        Create$parameters{namespace}ElementWrapperFunction function;
    };

    static constexpr TableEntry table[] = {
END
;

    for my $elementKey (sort keys %allElements) {
        # Do not add the name to the map if it does not have a JS wrapper constructor or uses the default wrapper.
        next if usesDefaultJSWrapper($elementKey) && ($parameters{fallbackJSInterfaceName} eq $parameters{namespace} . "Element");

        my $conditional = $allElements{$elementKey}{conditional};
        if ($conditional) {
            my $conditionalString = "ENABLE(" . join(") && ENABLE(", split(/&/, $conditional)) . ")";
            print F "#if ${conditionalString}\n";
        }

        my $ucName;
        if ($allElements{$elementKey}{settingsConditional}) {
            $ucName = $allElements{$elementKey}{interfaceName};
        } else {
            $ucName = $allElements{$elementKey}{JSInterfaceName};
        }

        print F "        { $parameters{namespace}Names::$allElements{$elementKey}{identifier}Tag, create${ucName}Wrapper },\n";

        if ($conditional) {
            print F "#endif\n";
        }
    }

    print F <<END
    };

    MemoryCompactLookupOnlyRobinHoodHashMap<AtomString, Create$parameters{namespace}ElementWrapperFunction> map;
    for (auto& entry : table)
        map.add(entry.name.get().localName(), entry.function);
    return map;
}

JSDOMObject* createJS$parameters{namespace}Wrapper(JSDOMGlobalObject* globalObject, Ref<$parameters{namespace}Element>&& element)
{
    static NeverDestroyed functions = create$parameters{namespace}WrapperMap();
    if (auto function = functions.get().get(element->localName()))
        return function(globalObject, WTFMove(element));
END
;

    if ($parameters{customElementInterfaceName}) {
        print F <<END
    if (!element->isUnknownElement())
        return createWrapper<$parameters{customElementInterfaceName}>(globalObject, WTFMove(element));
END
;
    }

    if ("$parameters{namespace}Element" eq $parameters{fallbackJSInterfaceName}) {
        print F <<END
    ASSERT(element->is$parameters{fallbackJSInterfaceName}());
END
;
    }

    print F <<END
    return createWrapper<$parameters{fallbackJSInterfaceName}>(globalObject, WTFMove(element));
}

}
END
;

    print F "\n#endif\n" if $parameters{guardFactoryWith};

    close F;
}

sub printWrapperFactoryHeaderFile
{
    my $outputDir = shift;
    my $wrapperFactoryFileName = shift;
    my $F;
    open F, ">" . $outputDir . "/JS" . $wrapperFactoryFileName . ".h";

    printLicenseHeader($F);

    print F "#pragma once\n\n";

    print F <<END
#include <wtf/Forward.h>

namespace WebCore {

    class JSDOMObject;
    class JSDOMGlobalObject;
    class $parameters{namespace}Element;

    JSDOMObject* createJS$parameters{namespace}Wrapper(JSDOMGlobalObject*, Ref<$parameters{namespace}Element>&&);

}
 
END
    ;

    close F;
}
