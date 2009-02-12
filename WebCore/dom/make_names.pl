#!/usr/bin/perl -w

# Copyright (C) 2005, 2006, 2007, 2009 Apple Inc. All rights reserved.
# Copyright (C) 2009, Julien Chaffraix <jchaffraix@webkit.org>
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

use strict;

use Config;
use Getopt::Long;
use File::Path;
use IO::File;
use InFilesParser;
use Switch;

my $printFactory = 0; 
my $printWrapperFactory = 0;
my $tagsFile = "";
my $attrsFile = "";
my $outputDir = ".";
my %tags = ();
my %attrs = ();
my %parameters = ();
my $extraDefines = 0;
my $preprocessor = "/usr/bin/gcc -E -P -x c++";

GetOptions('tags=s' => \$tagsFile, 
    'attrs=s' => \$attrsFile,
    'factory' => \$printFactory,
    'outputDir=s' => \$outputDir,
    'extraDefines=s' => \$extraDefines,
    'preprocessor=s' => \$preprocessor,
    'wrapperFactory' => \$printWrapperFactory);

die "You must specify at least one of --tags <file> or --attrs <file>" unless (length($tagsFile) || length($attrsFile));

readNames($tagsFile, "tags") if length($tagsFile);
readNames($attrsFile, "attrs") if length($attrsFile);

die "You must specify a namespace (e.g. SVG) for <namespace>Names.h" unless $parameters{'namespace'};
die "You must specify a namespaceURI (e.g. http://www.w3.org/2000/svg)" unless $parameters{'namespaceURI'};

$parameters{'namespacePrefix'} = $parameters{'namespace'} unless $parameters{'namespacePrefix'};

mkpath($outputDir);
my $namesBasePath = "$outputDir/$parameters{'namespace'}Names";
my $factoryBasePath = "$outputDir/$parameters{'namespace'}ElementFactory";
my $wrapperFactoryBasePath = "$outputDir/JS$parameters{'namespace'}ElementWrapperFactory";

printNamesHeaderFile("$namesBasePath.h");
printNamesCppFile("$namesBasePath.cpp");

if ($printFactory) {
    printFactoryCppFile("$factoryBasePath.cpp");
    printFactoryHeaderFile("$factoryBasePath.h");
}

if ($printWrapperFactory) {
    printWrapperFactoryCppFile("$wrapperFactoryBasePath.cpp");
    printWrapperFactoryHeaderFile("$wrapperFactoryBasePath.h");
}

### Hash initialization

sub initializeTagPropertyHash
{
    return ('constructorNeedsCreatedByParser' => 0,
            'constructorNeedsFormElement' => 0,
            'exportString' => 0,
            'interfaceName' => defaultInterfaceName($_[0]),
            # By default, the JSInterfaceName is the same as the interfaceName.
            'JSInterfaceName' => defaultInterfaceName($_[0]),
            'mapToTagName' => '',
            'wrapperOnlyIfMediaIsAvailable' => 0);
}

sub initializeAttrPropertyHash
{
    return ('exportString' => 0);
}

sub initializeParametersHash
{
    return ('namespace' => '',
            'namespacePrefix' => '',
            'namespaceURI' => '',
            'guardFactoryWith' => '',
            'tagsNullNamespace' => 0,
            'attrsNullNamespace' => 0,
            'exportStrings' => 0);
}

sub defaultInterfaceName
{
    die "No namespace found" if !$parameters{'namespace'};
    return $parameters{'namespace'} . upperCaseName($_[0]) . "Element"
}

### Parsing handlers

sub tagsHandler
{
    my ($tag, $property, $value) = @_;

    $tag =~ s/-/_/g;

    # Initialize default properties' values.
    $tags{$tag} = { initializeTagPropertyHash($tag) } if !defined($tags{$tag});

    if ($property) {
        die "Unknown property $property for tag $tag\n" if !defined($tags{$tag}{$property});
        # The code rely on JSInterfaceName deriving from interfaceName to check for custom JSInterfaceName.
        # So just override JSInterfaceName if it was not already set.
        if ($property eq "interfaceName" && $tags{$tag}{'JSInterfaceName'} eq $tags{$tag}{'interfaceName'}) {
                $tags{$tag}{'JSInterfaceName'} = $value;
        }
        $tags{$tag}{$property} = $value;
    }
}

sub attrsHandler
{
    my ($attr, $property, $value) = @_;

    $attr =~ s/-/_/g;

    # Initialize default properties' values.
    $attrs{$attr} = { initializeAttrPropertyHash($attr) } if !defined($attrs{$attr});

    if ($property) {
        die "Unknown property $property for attribute $attr\n" if !defined($attrs{$attr}{$property});
        $attrs{$attr}{$property} = $value;
    }
}

sub parametersHandler
{
    my ($parameter, $value) = @_;

    # Initialize default properties' values.
    %parameters = initializeParametersHash() if !(keys %parameters);

    die "Unknown parameter $parameter for tags/attrs\n" if !defined($parameters{$parameter});
    $parameters{$parameter} = $value;
}

## Support routines

sub readNames
{
    my ($namesFile, $type) = @_;

    my $names = new IO::File;

    if ($extraDefines eq 0) {
        open($names, $preprocessor . " " . $namesFile . "|") or die "Failed to open file: $namesFile";
    } else {
        open($names, $preprocessor . " -D" . join(" -D", split(" ", $extraDefines)) . " " . $namesFile . "|") or die "Failed to open file: $namesFile";
    }

    # Store hashes keys count to know if some insertion occured.
    my $tagsCount = keys %tags;
    my $attrsCount = keys %attrs;

    my $InParser = InFilesParser->new();

    switch ($type) {
        case "tags" {
            $InParser->parse($names, \&parametersHandler, \&tagsHandler);
        }
        case "attrs" {
            $InParser->parse($names, \&parametersHandler, \&attrsHandler);
        }
        else {
            die "Do not know how to parse $type";
        }
    }

    close($names);

    die "Failed to read names from file: $namesFile" if ((keys %tags == $tagsCount) && (keys %attrs == $attrsCount));
}

sub printMacros
{
    my ($F, $macro, $suffix, $namesRef) = @_;
    my %names = %$namesRef;

    for my $name (sort keys %$namesRef) {
        print F "$macro $name","$suffix;\n";

        if ($parameters{'exportStrings'} or $names{$name}{"exportString"}) { 
            print F "extern char $name", "${suffix}String[];\n";
        }
    }
}

sub usesDefaultWrapper
{
    my $tagName = shift;
    return $tagName eq $parameters{'namespace'} . "Element";
}

# Build a direct mapping from the tags to the Element to create, excluding
# Element that have not constructor.
sub buildConstructorMap
{
    my %tagConstructorMap = ();
    for my $tagName (keys %tags) {
        my $interfaceName = $tags{$tagName}{'interfaceName'};
        next if (usesDefaultWrapper($interfaceName));

        if ($tags{$tagName}{'mapToTagName'}) {
            die "Cannot handle multiple mapToTagName for $tagName\n" if $tags{$tags{$tagName}{'mapToTagName'}}{'mapToTagName'};
            $interfaceName = $tags{ $tags{$tagName}{'mapToTagName'} }{'interfaceName'};
        }

        # Chop the string to keep the interesting part.
        $interfaceName =~ s/$parameters{'namespace'}(.*)Element/$1/;
        $tagConstructorMap{$tagName} = lc($interfaceName);
    }

    return %tagConstructorMap;
}

# Helper method that print the constructor's signature avoiding
# unneeded arguments.
sub printConstructorSignature
{
    my ($F, $tagName, $constructorName, $constructorTagName) = @_;

    print F "static PassRefPtr<$parameters{'namespace'}Element> ${constructorName}Constructor(const QualifiedName& $constructorTagName, Document* doc";
    if ($parameters{'namespace'} eq "HTML") {
        print F ", HTMLFormElement*";
        if ($tags{$tagName}{'constructorNeedsFormElement'}) {
            print F " formElement";
        }
    }
    print F ", bool";
    if ($tags{$tagName}{'constructorNeedsCreatedByParser'}) {
        print F " createdByParser"; 
    }
    print F ")\n{\n";
}

# Helper method to dump the constructor interior and call the 
# Element constructor with the right arguments.
# The variable names should be kept in sync with the previous method.
sub printConstructorInterior
{
    my ($F, $tagName, $interfaceName, $constructorTagName) = @_;

    # Handle media elements.
    if ($tags{$tagName}{'wrapperOnlyIfMediaIsAvailable'}) {
        print F <<END
    if (!MediaPlayer::isAvailable())
        return new HTMLElement($constructorTagName, doc);
END
;
    }

    # Now call the constructor with the right parameters.
    print F "    return new ${interfaceName}($constructorTagName, doc";
    if ($tags{$tagName}{'constructorNeedsFormElement'}) {
        print F ", formElement";
    }
    if ($tags{$tagName}{'constructorNeedsCreatedByParser'}) {
        print F ", createdByParser";
    }
    print F ");\n}\n\n";
}

sub printConstructors
{
    my ($F, $tagConstructorMapRef) = @_;
    my %tagConstructorMap = %$tagConstructorMapRef;

    print F "#if $parameters{'guardFactoryWith'}\n" if $parameters{'guardFactoryWith'};

    # This is to avoid generating the same constructor several times.
    my %uniqueTags = ();
    for my $tagName (sort keys %tagConstructorMap) {
        my $interfaceName = $tags{$tagName}{'interfaceName'};

        # Ignore the mapped tag
        # FIXME: It could be moved inside this loop but was split for readibility.
        next if (defined($uniqueTags{$interfaceName}) || $tags{$tagName}{'mapToTagName'});

        $uniqueTags{$interfaceName} = '1';

        printConstructorSignature($F, $tagName, $tagConstructorMap{$tagName}, "tagName");
        printConstructorInterior($F, $tagName, $interfaceName, "tagName");
    }

    # Mapped tag name uses a special wrapper to keep their prefix and namespaceURI while using the mapped localname.
    for my $tagName (sort keys %tagConstructorMap) {
        if ($tags{$tagName}{'mapToTagName'}) {
            my $mappedName = $tags{$tagName}{'mapToTagName'};
            printConstructorSignature($F, $mappedName, $mappedName . "To" . $tagName, "tagName");
            printConstructorInterior($F, $mappedName, $tags{$mappedName}{'interfaceName'}, "QualifiedName(tagName.prefix(), ${mappedName}Tag.localName(), tagName.namespaceURI())");
        }
    }

    print F "#endif\n" if $parameters{'guardFactoryWith'};
}

sub printFunctionInits
{
    my ($F, $tagConstructorMap) = @_;
    my %tagConstructorMap = %$tagConstructorMap;

    for my $tagName (sort keys %tagConstructorMap) {
        if ($tags{$tagName}{'mapToTagName'}) {
            print F "    addTag(${tagName}Tag, $tags{$tagName}{'mapToTagName'}To${tagName}Constructor);\n";
        } else {
            print F "    addTag(${tagName}Tag, $tagConstructorMap{$tagName}Constructor);\n";
        }
    }
}

sub svgCapitalizationHacks
{
    my $name = shift;

    if ($name =~ /^fe(.+)$/) {
        $name = "FE" . ucfirst $1;
    }

    return $name;
}

sub upperCaseName
{
    my $name = shift;
    
    $name = svgCapitalizationHacks($name) if ($parameters{'namespace'} eq "SVG");

    while ($name =~ /^(.*?)_(.*)/) {
        $name = $1 . ucfirst $2;
    }
    
    return ucfirst $name;
}

sub printLicenseHeader
{
    my $F = shift;
    print F "/*
 * THIS FILE IS AUTOMATICALLY GENERATED, DO NOT EDIT.
 *
 *
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

sub printNamesHeaderFile
{
    my ($headerPath) = shift;
    my $F;
    open F, ">$headerPath";
    
    printLicenseHeader($F);
    print F "#ifndef DOM_$parameters{'namespace'}NAMES_H\n";
    print F "#define DOM_$parameters{'namespace'}NAMES_H\n\n";
    print F "#include \"QualifiedName.h\"\n\n";
    
    print F "namespace WebCore {\n\n namespace $parameters{'namespace'}Names {\n\n";
    
    my $lowerNamespace = lc($parameters{'namespacePrefix'});
    print F "#ifndef DOM_$parameters{'namespace'}NAMES_HIDE_GLOBALS\n";
    print F "// Namespace\n";
    print F "extern const WebCore::AtomicString ${lowerNamespace}NamespaceURI;\n\n";

    if (keys %tags) {
        print F "// Tags\n";
        printMacros($F, "extern const WebCore::QualifiedName", "Tag", \%tags);
    }
    
    if (keys %attrs) {
        print F "// Attributes\n";
        printMacros($F, "extern const WebCore::QualifiedName", "Attr", \%attrs);
    }
    print F "#endif\n\n";
    
    if (keys %tags) {
        print F "WebCore::QualifiedName** get$parameters{'namespace'}Tags(size_t* size);\n";
    }

    if (keys %attrs) {
        print F "WebCore::QualifiedName** get$parameters{'namespace'}Attrs(size_t* size);\n";
    }
    
    print F "\nvoid init();\n\n";
    print F "} }\n\n";
    print F "#endif\n\n";
    
    close F;
}

sub printNamesCppFile
{
    my $cppPath = shift;
    my $F;
    open F, ">$cppPath";
    
    printLicenseHeader($F);
    
    my $lowerNamespace = lc($parameters{'namespacePrefix'});

print F "#include \"config.h\"\n";

print F "#ifdef SKIP_STATIC_CONSTRUCTORS_ON_GCC\n";
print F "#define DOM_$parameters{'namespace'}NAMES_HIDE_GLOBALS 1\n";
print F "#else\n";
print F "#define QNAME_DEFAULT_CONSTRUCTOR 1\n";
print F "#endif\n\n";


print F "#include \"$parameters{'namespace'}Names.h\"\n\n";
print F "#include \"StaticConstructors.h\"\n";

print F "namespace WebCore {\n\n namespace $parameters{'namespace'}Names {

using namespace WebCore;

DEFINE_GLOBAL(AtomicString, ${lowerNamespace}NamespaceURI, \"$parameters{'namespaceURI'}\")
";

    if (keys %tags) {
        print F "// Tags\n";
        for my $name (sort keys %tags) {
            print F "DEFINE_GLOBAL(QualifiedName, ", $name, "Tag, nullAtom, \"$name\", ${lowerNamespace}NamespaceURI);\n";
        }
        
        print F "\n\nWebCore::QualifiedName** get$parameters{'namespace'}Tags(size_t* size)\n";
        print F "{\n    static WebCore::QualifiedName* $parameters{'namespace'}Tags[] = {\n";
        for my $name (sort keys %tags) {
            print F "        (WebCore::QualifiedName*)&${name}Tag,\n";
        }
        print F "    };\n";
        print F "    *size = ", scalar(keys %tags), ";\n";
        print F "    return $parameters{'namespace'}Tags;\n";
        print F "}\n";
    }

    if (keys %attrs) {
        print F "\n// Attributes\n";
        for my $name (sort keys %attrs) {
            print F "DEFINE_GLOBAL(QualifiedName, ", $name, "Attr, nullAtom, \"$name\", ${lowerNamespace}NamespaceURI);\n";
        }
        print F "\n\nWebCore::QualifiedName** get$parameters{'namespace'}Attrs(size_t* size)\n";
        print F "{\n    static WebCore::QualifiedName* $parameters{'namespace'}Attr[] = {\n";
        for my $name (sort keys %attrs) {
            print F "        (WebCore::QualifiedName*)&${name}Attr,\n";
        }
        print F "    };\n";
        print F "    *size = ", scalar(keys %attrs), ";\n";
        print F "    return $parameters{'namespace'}Attr;\n";
        print F "}\n";
    }

    if (keys %tags) {
        printDefinitionStrings($F, \%tags, "tags");
    }

    if (keys %attrs) {
        printDefinitionStrings($F, \%attrs, "attributes");
    }

print F "\nvoid init()
{
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;
    
    // Use placement new to initialize the globals.
    
    AtomicString::init();
";
    
    print(F "    AtomicString ${lowerNamespace}NS(\"$parameters{'namespaceURI'}\");\n\n");

    print(F "    // Namespace\n");
    print(F "    new ((void*)&${lowerNamespace}NamespaceURI) AtomicString(${lowerNamespace}NS);\n\n");
    if (keys %tags) {
        my $tagsNamespace = $parameters{'tagsNullNamespace'} ? "nullAtom" : "${lowerNamespace}NS";
        printDefinitions($F, \%tags, "tags", $tagsNamespace);
    }
    if (keys %attrs) {
        my $attrsNamespace = $parameters{'attrsNullNamespace'} ? "nullAtom" : "${lowerNamespace}NS";
        printDefinitions($F, \%attrs, "attributes", $attrsNamespace);
    }

    print F "}\n\n} }\n\n";
    close F;
}

sub printJSElementIncludes
{
    my $F = shift;

    my %tagsSeen;
    for my $tagName (sort keys %tags) {
        my $JSInterfaceName = $tags{$tagName}{"JSInterfaceName"};
        next if defined($tagsSeen{$JSInterfaceName}) || usesDefaultJSWrapper($tagName);
        $tagsSeen{$JSInterfaceName} = 1;

        print F "#include \"JS${JSInterfaceName}.h\"\n";
    }
}

sub printElementIncludes
{
    my $F = shift;

    my %tagsSeen;
    for my $tagName (sort keys %tags) {
        my $interfaceName = $tags{$tagName}{"interfaceName"};
        next if defined($tagsSeen{$interfaceName});
        $tagsSeen{$interfaceName} = 1;

        print F "#include \"${interfaceName}.h\"\n";
    }
}

sub printDefinitionStrings
{
    my ($F, $namesRef, $type) = @_;
    my $singularType = substr($type, 0, -1);
    my $shortType = substr($singularType, 0, 4);
    my $shortCamelType = ucfirst($shortType);
    print F "\n// " . ucfirst($type) . " as strings\n";

    my %names = %$namesRef;
    for my $name (sort keys %$namesRef) {
        next if (!$parameters{'exportStrings'} and !$names{$name}{"exportString"});

        my $realName = $name;
        $realName =~ s/_/-/g;

        print F "char $name","${shortCamelType}String[] = \"$realName\";\n";
    }
} 

sub printDefinitions
{
    my ($F, $namesRef, $type, $namespaceURI) = @_;
    my $singularType = substr($type, 0, -1);
    my $shortType = substr($singularType, 0, 4);
    my $shortCamelType = ucfirst($shortType);
    my $shortUpperType = uc($shortType);
    
    print F "    // " . ucfirst($type) . "\n";

    my %names = %$namesRef;
    for my $name (sort keys %$namesRef) {
        next if ($parameters{'exportStrings'} or $names{$name}{"exportString"});

        my $realName = $name;
        $realName =~ s/_/-/g;
        print F "    const char *$name","${shortCamelType}String = \"$realName\";\n";
    }

    print "\n";

    for my $name (sort keys %$namesRef) {
        print F "    new ((void*)&$name","${shortCamelType}) QualifiedName(nullAtom, $name","${shortCamelType}String, $namespaceURI);\n";
    }
}

## ElementFactory routines

sub printFactoryCppFile
{
    my $cppPath = shift;
    my $F;
    open F, ">$cppPath";

printLicenseHeader($F);

print F <<END
#include "config.h"
#include "$parameters{'namespace'}ElementFactory.h"

#include "$parameters{'namespace'}Names.h"
#if ENABLE(DASHBOARD_SUPPORT)
#include "Document.h"
#include "Settings.h"
#endif

END
;

if ($parameters{'namespace'} eq "HTML") {
    print F "#include \"HTMLFormElement.h\"\n";
}

printElementIncludes($F);

print F <<END
#include <wtf/HashMap.h>

namespace WebCore {

using namespace $parameters{'namespace'}Names;

END
;

print F "typedef PassRefPtr<$parameters{'namespace'}Element> (*ConstructorFunction)(const QualifiedName&, Document*";

if ($parameters{'namespace'} eq "HTML") {
    print F ", HTMLFormElement*";
}

print F ", bool createdByParser);\n";
print F <<END
typedef HashMap<AtomicStringImpl*, ConstructorFunction> FunctionMap;

static FunctionMap* gFunctionMap = 0;

END
;

my %tagConstructorMap = buildConstructorMap();

printConstructors($F, \%tagConstructorMap);

print F "#if $parameters{'guardFactoryWith'}\n" if $parameters{'guardFactoryWith'};

print F <<END
static void addTag(const QualifiedName& tag, ConstructorFunction func)
{
    gFunctionMap->set(tag.localName().impl(), func);
}

static inline void createFunctionMapIfNecessary()
{
    if (gFunctionMap)
        return;
    // Create the table.
    gFunctionMap = new FunctionMap;
    
    // Populate it with constructor functions.
END
;

printFunctionInits($F, \%tagConstructorMap);

print F "}\n";
print F "#endif\n" if $parameters{'guardFactoryWith'};

print F "\nPassRefPtr<$parameters{'namespace'}Element> $parameters{'namespace'}ElementFactory::create$parameters{'namespace'}Element(const QualifiedName& qName, Document* doc";

if ($parameters{"namespace"} eq "HTML") {
    print F ", HTMLFormElement* formElement";
}

print F ", bool createdByParser)\n{\n";

print F "#if $parameters{'guardFactoryWith'}\n" if $parameters{'guardFactoryWith'};

print F <<END
    // Don't make elements without a document
    if (!doc)
        return 0;

END
;

if ($parameters{'namespace'} ne "HTML") {
print F <<END
#if ENABLE(DASHBOARD_SUPPORT)
    Settings* settings = doc->settings();
    if (settings && settings->usesDashboardBackwardCompatibilityMode())
        return 0;
#endif
END
;

}

print F <<END
    createFunctionMapIfNecessary();
    ConstructorFunction func = gFunctionMap->get(qName.localName().impl());
    if (func)
END
;

if ($parameters{"namespace"} eq "HTML") {
    print F "        return func(qName, doc, formElement, createdByParser);\n";
} else {
    print F "        return func(qName, doc, createdByParser);\n";
}

print F "    return new $parameters{'namespace'}Element(qName, doc);\n";

if ($parameters{'guardFactoryWith'}) {

print F <<END
#else
    return 0;
#endif
END
;

}

print F <<END
}

} // namespace WebCore

END
;

    close F;
}

sub printFactoryHeaderFile
{
    my $headerPath = shift;
    my $F;
    open F, ">$headerPath";

    printLicenseHeader($F);

    print F<<END
#ifndef $parameters{'namespace'}ElementFactory_h
#define $parameters{'namespace'}ElementFactory_h

#include <wtf/PassRefPtr.h>

namespace WebCore {
    class Element;
    class Document;
    class QualifiedName;
    class AtomicString;
}

namespace WebCore {

    class $parameters{'namespace'}Element;
END
;

if ($parameters{'namespace'} eq "HTML") {
    print F "     class HTMLFormElement;\n";
}

print F<<END
    // The idea behind this class is that there will eventually be a mapping from namespace URIs to ElementFactories that can dispense
    // elements. In a compound document world, the generic createElement function (will end up being virtual) will be called.
    class $parameters{'namespace'}ElementFactory {
    public:
        PassRefPtr<Element> createElement(const WebCore::QualifiedName&, WebCore::Document*, bool createdByParser = true);
END
;
print F "        static PassRefPtr<$parameters{'namespace'}Element> create$parameters{'namespace'}Element(const WebCore::QualifiedName&, WebCore::Document*";

if ($parameters{'namespace'} eq "HTML") {
    print F ", HTMLFormElement* = 0";
}

print F ", bool /*createdByParser*/ = true);\n";

printf F<<END
    };
}

#endif // $parameters{'namespace'}ElementFactory_h

END
;

    close F;
}

## Wrapper Factory routines

sub usesDefaultJSWrapper
{
    my $name = shift;

    # A tag reuses the default wrapper if its JSInterfaceName matches the default namespace Element.
    return $tags{$name}{'JSInterfaceName'} eq $parameters{"namespace"} . "Element";
}

sub printWrapperFunctions
{
    my $F = shift;

    my %tagsSeen;
    for my $tagName (sort keys %tags) {
        # Avoid defining the same wrapper method twice.
        my $JSInterfaceName = $tags{$tagName}{"JSInterfaceName"};
        next if defined($tagsSeen{$JSInterfaceName}) || usesDefaultJSWrapper($tagName);
        $tagsSeen{$JSInterfaceName} = 1;

        # Hack for the media tags
        if ($tags{$tagName}{"wrapperOnlyIfMediaIsAvailable"}) {
            print F <<END
static JSNode* create${JSInterfaceName}Wrapper(ExecState* exec, PassRefPtr<$parameters{'namespace'}Element> element)
{
    if (!MediaPlayer::isAvailable())
        return CREATE_DOM_NODE_WRAPPER(exec, $parameters{'namespace'}Element, element.get());
    return CREATE_DOM_NODE_WRAPPER(exec, ${JSInterfaceName}, element.get());
}

END
;
        } else {
            print F <<END
static JSNode* create${JSInterfaceName}Wrapper(ExecState* exec, PassRefPtr<$parameters{'namespace'}Element> element)
{
    return CREATE_DOM_NODE_WRAPPER(exec, ${JSInterfaceName}, element.get());
}

END
;
        }
    }
}

sub printWrapperFactoryCppFile
{
    my $cppPath = shift;
    my $F;
    open F, ">$cppPath";

    printLicenseHeader($F);

    print F "#include \"config.h\"\n\n";

    print F "#if $parameters{'guardFactoryWith'}\n\n" if $parameters{'guardFactoryWith'};

    print F "#include \"JS$parameters{'namespace'}ElementWrapperFactory.h\"\n";

    printJSElementIncludes($F);

    print F "\n#include \"$parameters{'namespace'}Names.h\"\n\n";

    printElementIncludes($F);

    print F "\n#include <wtf/StdLibExtras.h>\n\n";
    
    print F <<END
using namespace JSC;

namespace WebCore {

using namespace $parameters{'namespace'}Names;

typedef JSNode* (*Create$parameters{'namespace'}ElementWrapperFunction)(ExecState*, PassRefPtr<$parameters{'namespace'}Element>);

END
;

    printWrapperFunctions($F);

    print F <<END
JSNode* createJS$parameters{'namespace'}Wrapper(ExecState* exec, PassRefPtr<$parameters{'namespace'}Element> element)
{   
    typedef HashMap<WebCore::AtomicStringImpl*, Create$parameters{'namespace'}ElementWrapperFunction> FunctionMap;
    DEFINE_STATIC_LOCAL(FunctionMap, map, ());
    if (map.isEmpty()) {
END
;

    for my $tag (sort keys %tags) {
        # Do not add the name to the map if it does not have a JS wrapper constructor or uses the default wrapper.
        next if usesDefaultJSWrapper($tag, \%tags);

        my $ucTag = $tags{$tag}{"JSInterfaceName"};
        print F "       map.set(${tag}Tag.localName().impl(), create${ucTag}Wrapper);\n";
    }

    print F <<END
    }
    Create$parameters{'namespace'}ElementWrapperFunction createWrapperFunction = map.get(element->localName().impl());
    if (createWrapperFunction)
        return createWrapperFunction(exec, element);
    return CREATE_DOM_NODE_WRAPPER(exec, $parameters{'namespace'}Element, element.get());
}

}

END
;

    print F "#endif\n" if $parameters{'guardFactoryWith'};

    close F;
}

sub printWrapperFactoryHeaderFile
{
    my $headerPath = shift;
    my $F;
    open F, ">$headerPath";

    printLicenseHeader($F);

    print F "#ifndef JS$parameters{'namespace'}ElementWrapperFactory_h\n";
    print F "#define JS$parameters{'namespace'}ElementWrapperFactory_h\n\n";

    print F "#if $parameters{'guardFactoryWith'}\n" if $parameters{'guardFactoryWith'};

    print F <<END
#include <wtf/Forward.h>

namespace JSC {
    class ExecState;
}                                            
                                             
namespace WebCore {

    class JSNode;
    class $parameters{'namespace'}Element;

    JSNode* createJS$parameters{'namespace'}Wrapper(JSC::ExecState*, PassRefPtr<$parameters{'namespace'}Element>);

}
 
END
;

    print F "#endif // $parameters{'guardFactoryWith'}\n\n" if $parameters{'guardFactoryWith'};

    print F "#endif // JS$parameters{'namespace'}ElementWrapperFactory_h\n";

    close F;
}
