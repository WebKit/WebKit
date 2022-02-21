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

sub readTags($);
sub readAttrs($);

my $printFactory = 0; 
my $printWrapperFactory = 0; 
my $fontNamesIn = "";
my $tagsFile = "";
my $attrsFile = "";
my $outputDir = ".";
my %parsedTags = ();
my %parsedAttrs = ();
my %allTags = ();
my %allAttrs = ();
my %allStrings = ();
my %parameters = ();
my $initDefaults = 1;
my %extensionAttrs = ();

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
    'tags=s' => \$tagsFile, 
    'attrs=s' => \$attrsFile,
    'factory' => \$printFactory,
    'outputDir=s' => \$outputDir,
    'wrapperFactory' => \$printWrapperFactory,
    'fonts=s' => \$fontNamesIn
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
    print F "#endif\n\n";

    printInit($F, 1);
    close F;

    my $source = File::Spec->catfile($outputDir, "${familyNamesFileBase}Names.cpp");
    open F, ">$source" or die "Unable to open $source for writing.";

    printLicenseHeader($F);
    printCppHead($F, "CSS", $familyNamesFileBase, "WTF");

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

die "You must specify at least one of --tags <file> or --attrs <file>" unless (length($tagsFile) || length($attrsFile));

if (length($tagsFile)) {
    %allTags = %{readTags($tagsFile)};
    namesToStrings(\%allTags, \%allStrings);
}

if (length($attrsFile)) {
    %allAttrs = %{readAttrs($attrsFile)};
    namesToStrings(\%allAttrs, \%allStrings);
}

die "You must specify a namespace (e.g. SVG) for <namespace>Names.h" unless $parameters{namespace};
die "You must specify a namespaceURI (e.g. http://www.w3.org/2000/svg)" unless $parameters{namespaceURI};

$parameters{namespacePrefix} = $parameters{namespace} unless $parameters{namespacePrefix};
$parameters{fallbackJSInterfaceName} = $parameters{fallbackInterfaceName} unless $parameters{fallbackJSInterfaceName};

my $typeHelpersBasePath = "$outputDir/$parameters{namespace}ElementTypeHelpers";
my $namesBasePath = "$outputDir/$parameters{namespace}Names";
my $factoryBasePath = "$outputDir/$parameters{namespace}ElementFactory";
my $wrapperFactoryFileName = "$parameters{namespace}ElementWrapperFactory";

printNamesHeaderFile("$namesBasePath.h");
printNamesCppFile("$namesBasePath.cpp");
printTypeHelpersHeaderFile("$typeHelpersBasePath.h");

if ($printFactory) {
    printFactoryCppFile("$factoryBasePath.cpp");
    printFactoryHeaderFile("$factoryBasePath.h");
}

if ($printWrapperFactory) {
    printWrapperFactoryCppFile($outputDir, $wrapperFactoryFileName);
    printWrapperFactoryHeaderFile($outputDir, $wrapperFactoryFileName);
}

### Hash initialization

sub defaultTagPropertyHash
{
    return (
        'constructorNeedsCreatedByParser' => 0,
        'constructorNeedsFormElement' => 0,
        'noConstructor' => 0,
        'interfaceName' => defaultInterfaceName($_[0]),
        # By default, the JSInterfaceName is the same as the interfaceName.
        'JSInterfaceName' => defaultInterfaceName($_[0]),
        'mapToTagName' => '',
        'wrapperOnlyIfMediaIsAvailable' => 0,
        'settingsConditional' => 0,
        'conditional' => 0,
        'runtimeEnabled' => 0,
        'customTypeHelper' => 0,
    );
}

sub defaultParametersHash
{
    return (
        'namespace' => '',
        'namespacePrefix' => '',
        'namespaceURI' => '',
        'guardFactoryWith' => '',
        'tagsNullNamespace' => 0,
        'attrsNullNamespace' => 0,
        'fallbackInterfaceName' => '',
        'fallbackJSInterfaceName' => '',
        'customElementInterfaceName' => '',
    );
}

sub defaultInterfaceName
{
    die "No namespace found" if !$parameters{namespace};
    return $parameters{namespace} . upperCaseName($_[0]) . "Element"
}

### Parsing handlers

sub valueForName
{
    my $name = shift;
    my $value = $extensionAttrs{$name};

    if (!$value) {
        $value = $name;
        $value =~ s/_/-/g;
    }

    return $value;
}

sub namesToStrings
{
    my $namesRef = shift;
    my $stringsRef = shift;

    my %names = %$namesRef;

    for my $name (keys %names) {
        $stringsRef->{$name} = valueForName($name);
    }
}

sub tagsHandler
{
    my ($tag, $property, $value) = @_;

    $tag =~ s/-/_/g;

    # Initialize default property values.
    $parsedTags{$tag} = { defaultTagPropertyHash($tag) } if !defined($parsedTags{$tag});

    if ($property) {
        die "Unknown property $property for tag $tag\n" if !defined($parsedTags{$tag}{$property});

        # The code relies on JSInterfaceName deriving from interfaceName to check for custom JSInterfaceName.
        # So override JSInterfaceName if it was not already set.
        $parsedTags{$tag}{JSInterfaceName} = $value if $property eq "interfaceName" && $parsedTags{$tag}{JSInterfaceName} eq $parsedTags{$tag}{interfaceName};

        $parsedTags{$tag}{$property} = $value;
    }
}

sub attrsHandler
{
    my ($attr, $property, $value) = @_;
    # Translate HTML5 extension attributes of the form 'x-webkit-feature' to 'webkitfeature'.
    # We don't just check for the 'x-' prefix because there are attributes such as x-height
    # which should follow the default path below.
    if ($attr =~ m/^x-webkit-(.*)/) {
        my $newAttr = "webkit$1";
        $extensionAttrs{$newAttr} = $attr;
        $attr = $newAttr;
    }
    $attr =~ s/-/_/g;

    # Initialize default properties' values.
    $parsedAttrs{$attr} = {} if !defined($parsedAttrs{$attr});

    if ($property) {
        die "Unknown property $property for attribute $attr\n" if !defined($parsedAttrs{$attr}{$property});
        $parsedAttrs{$attr}{$property} = $value;
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

    close($names);
    die "Failed to read names from file: $namesFile" if (keys %{$hashToFillRef} == 0);
    return $hashToFillRef;
}

sub readAttrs($)
{
    my ($namesFile) = @_;
    %parsedAttrs = ();
    return readNames($namesFile, \%parsedAttrs, \&attrsHandler);
}

sub readTags($)
{
    my ($namesFile) = @_;
    %parsedTags = ();
    return readNames($namesFile, \%parsedTags, \&tagsHandler);
}

sub printMacros
{
    my ($F, $macro, $suffix, $namesRef) = @_;
    my %names = %$namesRef;

    for my $name (sort keys %names) {
        print F "$macro $name","$suffix;\n";
    }
}

sub usesDefaultWrapper
{
    my $tagName = shift;
    return $tagName eq $parameters{namespace} . "Element";
}

# Build a direct mapping from the tags to the Element to create.
sub buildConstructorMap
{
    my %tagConstructorMap = ();
    for my $tagName (keys %allTags) {
        my $interfaceName = $allTags{$tagName}{interfaceName};

        if ($allTags{$tagName}{mapToTagName}) {
            die "Cannot handle multiple mapToTagName for $tagName\n" if $allTags{$allTags{$tagName}{mapToTagName}}{mapToTagName};
            $interfaceName = $allTags{ $allTags{$tagName}{mapToTagName} }{interfaceName};
        }

        # Chop the string to keep the interesting part.
        $interfaceName =~ s/$parameters{namespace}(.*)Element/$1/;
        $tagConstructorMap{$tagName} = lc($interfaceName);
    }

    return %tagConstructorMap;
}

# Helper method that print the constructor's signature avoiding
# unneeded arguments.
sub printConstructorSignature
{
    my ($F, $tagName, $constructorName, $constructorTagName) = @_;

    print F "static Ref<$parameters{namespace}Element> ${constructorName}Constructor(const QualifiedName& $constructorTagName, Document& document";
    if ($parameters{namespace} eq "HTML") {
        print F ", HTMLFormElement*";
        print F " formElement" if $allTags{$tagName}{constructorNeedsFormElement};
    }
    print F ", bool";
    print F " createdByParser" if $allTags{$tagName}{constructorNeedsCreatedByParser};
    print F ")\n{\n";
}

# Helper method to dump the constructor interior and call the 
# Element constructor with the right arguments.
# The variable names should be kept in sync with the previous method.
sub printConstructorInterior
{
    my ($F, $tagName, $interfaceName, $constructorTagName) = @_;

    # Handle media elements.
    # Note that wrapperOnlyIfMediaIsAvailable is a misnomer, because media availability
    # does not just control the wrapper; it controls the element object that is created.
    # FIXME: Could we instead do this entirely in the wrapper, and use custom wrappers
    # instead of having all the support for this here in this script?
    if ($allTags{$tagName}{wrapperOnlyIfMediaIsAvailable}) {
        print F <<END
    if (!document.settings().mediaEnabled())
        return $parameters{fallbackInterfaceName}::create($constructorTagName, document);
    
END
;
    }

    my $runtimeCondition;
    my $settingsConditional = $allTags{$tagName}{settingsConditional};
    my $runtimeEnabled = $allTags{$tagName}{runtimeEnabled};
    if ($settingsConditional) {
        $runtimeCondition = "document.settings().${settingsConditional}()";
    } elsif ($runtimeEnabled) {
        $runtimeCondition = "RuntimeEnabledFeatures::sharedFeatures().${runtimeEnabled}Enabled()";
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
    print F ", formElement" if $allTags{$tagName}{constructorNeedsFormElement};
    print F ", createdByParser" if $allTags{$tagName}{constructorNeedsCreatedByParser};
    print F ");\n}\n";
}

sub printConstructors
{
    my ($F, $tagConstructorMapRef) = @_;
    my %tagConstructorMap = %$tagConstructorMapRef;

    # This is to avoid generating the same constructor several times.
    my %uniqueTags = ();
    for my $tagName (sort keys %tagConstructorMap) {
        my $interfaceName = $allTags{$tagName}{interfaceName};

        # Ignore the mapped tag
        # FIXME: It could be moved inside this loop but was split for readibility.
        next if (defined($uniqueTags{$interfaceName}) || $allTags{$tagName}{mapToTagName});
        # Tags can have wrappers without constructors.
        # This is useful to make user-agent shadow elements internally testable
        # while keeping them from being avaialble in the HTML markup.
        next if $allTags{$tagName}{noConstructor};

        $uniqueTags{$interfaceName} = '1';

        my $conditional = $allTags{$tagName}{conditional};
        if ($conditional) {
            my $conditionalString = "ENABLE(" . join(") && ENABLE(", split(/&/, $conditional)) . ")";
            print F "#if ${conditionalString}\n";
        }

        printConstructorSignature($F, $tagName, $tagConstructorMap{$tagName}, "tagName");
        printConstructorInterior($F, $tagName, $interfaceName, "tagName");

        if ($conditional) {
            print F "#endif\n";
        }

        print F "\n";
    }

    # Mapped tag name uses a special wrapper to keep their prefix and namespaceURI while using the mapped localname.
    for my $tagName (sort keys %tagConstructorMap) {
        if ($allTags{$tagName}{mapToTagName}) {
            my $mappedName = $allTags{$tagName}{mapToTagName};
            printConstructorSignature($F, $mappedName, $mappedName . "To" . $tagName, "tagName");
            printConstructorInterior($F, $mappedName, $allTags{$mappedName}{interfaceName}, "QualifiedName(tagName.prefix(), ${mappedName}Tag->localName(), tagName.namespaceURI())");
        }
    }
}

sub printFunctionTable
{
    my ($F, $tagConstructorMap) = @_;
    my %tagConstructorMap = %$tagConstructorMap;

    for my $tagName (sort keys %tagConstructorMap) {
        next if $allTags{$tagName}{noConstructor};

        my $conditional = $allTags{$tagName}{conditional};
        if ($conditional) {
            my $conditionalString = "ENABLE(" . join(") && ENABLE(", split(/&/, $conditional)) . ")";
            print F "#if ${conditionalString}\n";
        }

        if ($allTags{$tagName}{mapToTagName}) {
            print F "        { $parameters{namespace}Names::${tagName}Tag, $allTags{$tagName}{mapToTagName}To${tagName}Constructor },\n";
        } else {
            print F "        { $parameters{namespace}Names::${tagName}Tag, $tagConstructorMap{$tagName}Constructor },\n";
        }

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

#ifndef ${prefix}_${namespace}_NAMES_HIDE_GLOBALS

END
    ;
}

sub printCppHead
{
    my ($F, $prefix, $namespace, $usedNamespace) = @_;

    print F "#include \"config.h\"\n\n";
    print F "#ifdef SKIP_STATIC_CONSTRUCTORS_ON_GCC\n";
    print F "#define ${prefix}_${namespace}_NAMES_HIDE_GLOBALS 1\n";
    print F "#else\n";
    print F "#define QNAME_DEFAULT_CONSTRUCTOR 1\n";
    print F "#endif\n\n";

    print F "#include \"${namespace}Names.h\"\n\n";

    print F "namespace WebCore {\n\n";
    print F "namespace ${namespace}Names {\n\n";
    print F "using namespace $usedNamespace;\n\n";
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

    AtomString::init();
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
    my %names = %$namesRef;

    # Do a first pass to discard classes that map to several tags.
    my %classToTags = ();
    for my $name (keys %names) {
        my $class = $parsedTags{$name}{interfaceName};
        push(@{$classToTags{$class}}, $name) if defined $class;
    }

    for my $class (sort keys %classToTags) {
        my $name = $classToTags{$class}[0];
        next if $parsedTags{$name}{customTypeHelper};
        # Skip classes that map to more than 1 tag.
        my $tagCount = scalar @{$classToTags{$class}};
        next if $tagCount > 1;

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
       if ($parameters{namespace} eq "HTML" && ($parsedTags{$name}{wrapperOnlyIfMediaIsAvailable} || $parsedTags{$name}{settingsConditional} || $parsedTags{$name}{runtimeEnabled})) {
           print F <<END
    static bool checkTagName(const WebCore::HTMLElement& element) { return !element.isHTMLUnknownElement() && element.hasTagName(WebCore::$parameters{namespace}Names::${name}Tag); }
    static bool checkTagName(const WebCore::Node& node) { return is<WebCore::HTMLElement>(node) && checkTagName(downcast<WebCore::HTMLElement>(node)); }
END
           ;
       } else {
           print F <<END
    static bool checkTagName(const WebCore::$parameters{namespace}Element& element) { return element.hasTagName(WebCore::$parameters{namespace}Names::${name}Tag); }
    static bool checkTagName(const WebCore::Node& node) { return node.hasTagName(WebCore::$parameters{namespace}Names::${name}Tag); }
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
    printTypeHelpers($F, \%allTags);

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

    if (keys %allTags) {
        print F "// Tags\n";
        printMacros($F, "WEBCORE_EXPORT extern LazyNeverDestroyed<const WebCore::$parameters{namespace}QualifiedName>", "Tag", \%allTags);
    }

    if (keys %allAttrs) {
        print F "// Attributes\n";
        printMacros($F, "WEBCORE_EXPORT extern LazyNeverDestroyed<const WebCore::QualifiedName>", "Attr", \%allAttrs);
    }
    print F "#endif\n\n";

    if (keys %allTags) {
        print F "const unsigned $parameters{namespace}TagsCount = ", scalar(keys %allTags), ";\n";
        print F "const WebCore::$parameters{namespace}QualifiedName* const* get$parameters{namespace}Tags();\n";
    }

    if (keys %allAttrs) {
        print F "const unsigned $parameters{namespace}AttrsCount = ", scalar(keys %allAttrs), ";\n";
        print F "const WebCore::QualifiedName* const* get$parameters{namespace}Attrs();\n";
    }

    printInit($F, 1);
    close F;
}

sub printNamesCppFile
{
    my $cppPath = shift;
    my $F;
    open F, ">$cppPath";
    
    printLicenseHeader($F);
    printCppHead($F, "DOM", $parameters{namespace}, "WebCore");
    
    my $lowercaseNamespacePrefix = lc($parameters{namespacePrefix});

    print F "MainThreadLazyNeverDestroyed<const AtomString> ${lowercaseNamespacePrefix}NamespaceURI;\n\n";

    print F StaticString::GenerateStrings(\%allStrings);

    if (keys %allTags) {
        print F "// Tags\n";
        for my $name (sort keys %allTags) {
            print F "WEBCORE_EXPORT LazyNeverDestroyed<const $parameters{namespace}QualifiedName> ${name}Tag;\n";
        }
        
        print F "\n\nconst WebCore::$parameters{namespace}QualifiedName* const* get$parameters{namespace}Tags()\n";
        print F "{\n    static const WebCore::$parameters{namespace}QualifiedName* const $parameters{namespace}Tags[] = {\n";
        for my $name (sort keys %allTags) {
            print F "        &${name}Tag.get(),\n";
        }
        print F "    };\n";
        print F "    return $parameters{namespace}Tags;\n";
        print F "}\n";
    }

    if (keys %allAttrs) {
        print F "\n// Attributes\n";
        for my $name (sort keys %allAttrs) {
            print F "WEBCORE_EXPORT LazyNeverDestroyed<const QualifiedName> ${name}Attr;\n";
        }
        print F "\n\nconst WebCore::QualifiedName* const* get$parameters{namespace}Attrs()\n";
        print F "{\n    static const WebCore::QualifiedName* const $parameters{namespace}Attrs[] = {\n";
        for my $name (sort keys %allAttrs) {
            print F "        &${name}Attr.get(),\n";
        }
        print F "    };\n";
        print F "    return $parameters{namespace}Attrs;\n";
        print F "}\n";
    }

    printInit($F, 0);

    print(F "    AtomString ${lowercaseNamespacePrefix}NS(\"$parameters{namespaceURI}\", AtomString::ConstructFromLiteral);\n\n");

    print(F "    // Namespace\n");
    print(F "    ${lowercaseNamespacePrefix}NamespaceURI.construct(${lowercaseNamespacePrefix}NS);\n");
    print(F "\n");
    print F StaticString::GenerateStringAsserts(\%allStrings);

    if (keys %allTags) {
        my $tagsNamespace = $parameters{tagsNullNamespace} ? "nullAtom()" : "${lowercaseNamespacePrefix}NS";
        printDefinitions($F, \%allTags, "tags", $tagsNamespace);
    }
    if (keys %allAttrs) {
        my $attrsNamespace = $parameters{attrsNullNamespace} ? "nullAtom()" : "${lowercaseNamespacePrefix}NS";
        printDefinitions($F, \%allAttrs, "attributes", $attrsNamespace);
    }

    print F "}\n\n} }\n\n";
    close F;
}

sub printJSElementIncludes
{
    my $F = shift;

    my %tagsSeen;
    for my $tagName (sort keys %allTags) {
        my $JSInterfaceName = $allTags{$tagName}{JSInterfaceName};
        next if defined($tagsSeen{$JSInterfaceName}) || usesDefaultJSWrapper($tagName);
        if ($allTags{$tagName}{conditional}) {
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
    for my $tagName (sort keys %allTags) {
        my $interfaceName = $allTags{$tagName}{interfaceName};
        next if defined($tagsSeen{$interfaceName});
        if ($allTags{$tagName}{conditional}) {
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

    for my $tagName (keys %allTags) {
        my $conditional = $allTags{$tagName}{conditional};
        my $interfaceName = $allTags{$tagName}{interfaceName};
        my $JSInterfaceName = $allTags{$tagName}{JSInterfaceName};

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
    my ($F, $namesRef, $type, $namespaceURI) = @_;

    my $shortCamelType = ucfirst(substr(substr($type, 0, -1), 0, 4));
    my $capitalizedType = ucfirst($type);
    
print F <<END;

    struct ${capitalizedType}TableEntry {
        LazyNeverDestroyed<const QualifiedName>* targetAddress;
        const StaticStringImpl& name;
    };

    static const ${capitalizedType}TableEntry ${type}Table[] = {
END

    my $cast = $type eq "tags" ? "(LazyNeverDestroyed<const QualifiedName>*)" : "";
    for my $name (sort keys %$namesRef) {
        print F "        { $cast&$name$shortCamelType, *(&${name}Data) },\n";
    }

print F <<END;
    };

    for (auto& entry : ${type}Table)
        entry.targetAddress->construct(nullAtom(), AtomString(&entry.name), $namespaceURI);
END

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

#include "Document.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
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

    my $firstTag;
    for my $tag (sort keys %tagConstructorMap) {
        $firstTag = $tag;
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
        decltype($parameters{namespace}Names::${firstTag}Tag)& name;
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

class $parameters{namespace}ElementFactory {
public:
END
;

print F "static RefPtr<$parameters{namespace}Element> createKnownElement(const AtomString&, Document&";
print F ", HTMLFormElement* = nullptr" if $parameters{namespace} eq "HTML";
print F ", bool createdByParser = false);\n";

print F "static RefPtr<$parameters{namespace}Element> createKnownElement(const QualifiedName&, Document&";
print F ", HTMLFormElement* = nullptr" if $parameters{namespace} eq "HTML";
print F ", bool createdByParser = false);\n";

print F "static Ref<$parameters{namespace}Element> createElement(const AtomString&, Document&";
print F ", HTMLFormElement* = nullptr" if $parameters{namespace} eq "HTML";
print F ", bool createdByParser = false);\n";

print F "static Ref<$parameters{namespace}Element> createElement(const QualifiedName&, Document&";
print F ", HTMLFormElement* = nullptr" if $parameters{namespace} eq "HTML";
print F ", bool createdByParser = false);\n";

printf F<<END
};

}

END
;

    close F;
}

## Wrapper Factory routines

sub usesDefaultJSWrapper
{
    my $name = shift;

    # A tag reuses the default wrapper if its JSInterfaceName matches the default namespace Element.
    return $allTags{$name}{JSInterfaceName} eq $parameters{namespace} . "Element";
}

sub printWrapperFunctions
{
    my $F = shift;

    my %tagsSeen;
    for my $tagName (sort keys %allTags) {
        # Avoid defining the same wrapper method twice.
        my $JSInterfaceName = $allTags{$tagName}{JSInterfaceName};
        next if (defined($tagsSeen{$JSInterfaceName}) || (usesDefaultJSWrapper($tagName) && ($parameters{fallbackJSInterfaceName} eq $parameters{namespace} . "Element"))) && !$allTags{$tagName}{settingsConditional};
        $tagsSeen{$JSInterfaceName} = 1;

        my $conditional = $allTags{$tagName}{conditional};
        if ($conditional) {
            my $conditionalString = "ENABLE(" . join(") && ENABLE(", split(/&/, $conditional)) . ")";
            print F "#if ${conditionalString}\n\n";
        }

        if ($allTags{$tagName}{wrapperOnlyIfMediaIsAvailable}) {
            print F <<END
static JSDOMObject* create${JSInterfaceName}Wrapper(JSDOMGlobalObject* globalObject, Ref<$parameters{namespace}Element>&& element)
{
    if (element->is$parameters{fallbackInterfaceName}())
        return createWrapper<$parameters{fallbackInterfaceName}>(globalObject, WTFMove(element));
    return createWrapper<${JSInterfaceName}>(globalObject, WTFMove(element));
}

END
            ;
        } elsif ($allTags{$tagName}{settingsConditional}) {
            print F <<END
static JSDOMObject* create$allTags{$tagName}{interfaceName}Wrapper(JSDOMGlobalObject* globalObject, Ref<$parameters{namespace}Element>&& element)
{
    if (element->is$parameters{fallbackInterfaceName}())
        return createWrapper<$parameters{fallbackInterfaceName}>(globalObject, WTFMove(element));
    return createWrapper<${JSInterfaceName}>(globalObject, WTFMove(element));
}

END
            ;
        } elsif ($allTags{$tagName}{runtimeEnabled}) {
            my $runtimeEnabled = $allTags{$tagName}{runtimeEnabled};
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

#include "Document.h"
#include "RuntimeEnabledFeatures.h"
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

    my $firstTag;
    for my $tag (sort keys %allTags) {
        $firstTag = $tag;
        last;
    }

print F <<END

static NEVER_INLINE MemoryCompactLookupOnlyRobinHoodHashMap<AtomString, Create$parameters{namespace}ElementWrapperFunction> create$parameters{namespace}WrapperMap()
{
    struct TableEntry {
        decltype($parameters{namespace}Names::${firstTag}Tag)& name;
        Create$parameters{namespace}ElementWrapperFunction function;
    };

    static constexpr TableEntry table[] = {
END
;

    for my $tag (sort keys %allTags) {
        # Do not add the name to the map if it does not have a JS wrapper constructor or uses the default wrapper.
        next if (usesDefaultJSWrapper($tag, \%allTags) && ($parameters{fallbackJSInterfaceName} eq $parameters{namespace} . "Element"));

        my $conditional = $allTags{$tag}{conditional};
        if ($conditional) {
            my $conditionalString = "ENABLE(" . join(") && ENABLE(", split(/&/, $conditional)) . ")";
            print F "#if ${conditionalString}\n";
        }

        my $ucTag;
        if ($allTags{$tag}{settingsConditional}) {
            $ucTag = $allTags{$tag}{interfaceName};
        } else {
            $ucTag = $allTags{$tag}{JSInterfaceName};
        }

        print F "        { $parameters{namespace}Names::${tag}Tag, create${ucTag}Wrapper },\n";

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
