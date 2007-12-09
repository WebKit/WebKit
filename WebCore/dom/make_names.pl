#!/usr/bin/perl -w

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

use strict;
use Getopt::Long;
use File::Path;
use Config;

my $printFactory = 0;
my $cppNamespace = "";
my $namespace = "";
my $namespacePrefix = "";
my $namespaceURI = "";
my $tagsFile = "";
my $attrsFile = "";
my $outputDir = ".";
my @tags = ();
my @attrs = ();
my $tagsNullNamespace = 0;
my $attrsNullNamespace = 0;
my $extraDefines = 0;
my $preprocessor = "/usr/bin/gcc -E -P -x c++";

GetOptions('tags=s' => \$tagsFile, 
    'attrs=s' => \$attrsFile,
    'outputDir=s' => \$outputDir,
    'namespace=s' => \$namespace,
    'namespacePrefix=s' => \$namespacePrefix,
    'namespaceURI=s' => \$namespaceURI,
    'cppNamespace=s' => \$cppNamespace,
    'factory' => \$printFactory,
    'tagsNullNamespace' => \$tagsNullNamespace,
    'attrsNullNamespace' => \$attrsNullNamespace,
    'extraDefines=s' => \$extraDefines,
    'preprocessor=s' => \$preprocessor);

die "You must specify a namespace (e.g. SVG) for <namespace>Names.h" unless $namespace;
die "You must specify a namespaceURI (e.g. http://www.w3.org/2000/svg)" unless $namespaceURI;
die "You must specify a cppNamespace (e.g. DOM) used for <cppNamespace>::<namespace>Names::fooTag" unless $cppNamespace;
die "You must specify at least one of --tags <file> or --attrs <file>" unless (length($tagsFile) || length($attrsFile));

$namespacePrefix = $namespace unless $namespacePrefix;

@tags = readNames($tagsFile) if length($tagsFile);
@attrs = readNames($attrsFile) if length($attrsFile);

mkpath($outputDir);
my $namesBasePath = "$outputDir/${namespace}Names";
my $factoryBasePath = "$outputDir/${namespace}ElementFactory";

printNamesHeaderFile("$namesBasePath.h");
printNamesCppFile("$namesBasePath.cpp");
if ($printFactory) {
    printFactoryCppFile("$factoryBasePath.cpp");
    printFactoryHeaderFile("$factoryBasePath.h");
}


## Support routines

sub readNames
{
    my $namesFile = shift;

    if ($extraDefines eq 0) {
        die "Failed to open file: $namesFile" unless open NAMES, $preprocessor . " " . $namesFile . "|" or die;
    } else {
        die "Failed to open file: $namesFile" unless open NAMES, $preprocessor . " -D" . join(" -D", split(" ", $extraDefines)) . " " . $namesFile . "|" or die;
    }

    my @names = ();
    while (<NAMES>) {
        next if (m/#/);
        next if (m/^[ \t]*$/);
        s/-/_/g;
        chomp $_;
        push @names, $_;
    }    
    close(NAMES);
    
    die "Failed to read names from file: $namesFile" unless (scalar(@names));
    
    return @names
}

sub printMacros
{
    my ($F, $macro, $suffix, @names) = @_;
    for my $name (@names) {
        print F "    $macro $name","$suffix;\n";
    }
}

sub printConstructors
{
    my ($F, @names) = @_;
    print F "#if ENABLE(SVG)\n";
    for my $name (@names) {
        my $upperCase = upperCaseName($name);
    
        print F "${namespace}Element *${name}Constructor(Document *doc, bool createdByParser)\n";
        print F "{\n";
        print F "    return new ${namespace}${upperCase}Element(${name}Tag, doc);\n";
        print F "}\n\n";
    }
    print F "#endif\n";
}

sub printFunctionInits
{
    my ($F, @names) = @_;
    for my $name (@names) {
        print F "    gFunctionMap->set(${name}Tag.localName().impl(), ${name}Constructor);\n";
    }
}

sub svgCapitalizationHacks
{
    my $name = shift;
    
    if ($name =~ /^fe(.+)$/) {
        $name = "FE" . ucfirst $1;
    }
    $name =~ s/kern/Kern/;
    $name =~ s/mpath/MPath/;
    $name =~ s/svg/SVG/;
    $name =~ s/tref/TRef/;
    $name =~ s/tspan/TSpan/;
    
    return $name;
}

sub upperCaseName
{
    my $name = shift;
    
    $name = svgCapitalizationHacks($name) if ($namespace eq "SVG");
    
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
    print F "#ifndef DOM_${namespace}NAMES_H\n";
    print F "#define DOM_${namespace}NAMES_H\n\n";
    print F "#include \"QualifiedName.h\"\n\n";
    
    print F "namespace $cppNamespace { namespace ${namespace}Names {\n\n";
    
    my $lowerNamespace = lc($namespacePrefix);
    print F "#ifndef DOM_${namespace}NAMES_HIDE_GLOBALS\n";
    print F "// Namespace\n";
    print F "extern const WebCore::AtomicString ${lowerNamespace}NamespaceURI;\n\n";

    if (scalar(@tags)) {
        print F "// Tags\n";
        printMacros($F, "extern const WebCore::QualifiedName", "Tag", @tags);
        print F "\n\nWebCore::QualifiedName** get${namespace}Tags(size_t* size);\n";
    }
    
    if (scalar(@attrs)) {
        print F "// Attributes\n";
        printMacros($F, "extern const WebCore::QualifiedName", "Attr", @attrs);
        print F "\n\nWebCore::QualifiedName** get${namespace}Attr(size_t* size);\n";
    }
    print F "#endif\n\n";
    print F "void init();\n\n";
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
    
    my $lowerNamespace = lc($namespacePrefix);

print F "#include \"config.h\"\n";

print F "#ifdef AVOID_STATIC_CONSTRUCTORS\n";
print F "#define DOM_${namespace}NAMES_HIDE_GLOBALS 1\n";
print F "#else\n";
print F "#define QNAME_DEFAULT_CONSTRUCTOR 1\n";
print F "#endif\n\n";


print F "#include \"${namespace}Names.h\"\n\n";
print F "#include \"StaticConstructors.h\"\n";

print F "namespace $cppNamespace { namespace ${namespace}Names {

using namespace WebCore;

DEFINE_GLOBAL(AtomicString, ${lowerNamespace}NamespaceURI, \"$namespaceURI\")
";

    if (scalar(@tags)) {
        print F "// Tags\n";
        for my $name (@tags) {
            print F "DEFINE_GLOBAL(QualifiedName, ", $name, "Tag, nullAtom, \"$name\", ${lowerNamespace}NamespaceURI);\n";
        }
        
        print F "\n\nWebCore::QualifiedName** get${namespace}Tags(size_t* size)\n";
        print F "{\n    static WebCore::QualifiedName* ${namespace}Tags[] = {\n";
        for my $name (@tags) {
            print F "        (WebCore::QualifiedName*)&${name}Tag,\n";
        }
        print F "    };\n";
        print F "    *size = ", scalar(@tags), ";\n";
        print F "    return ${namespace}Tags;\n";
        print F "}\n";
        
    }

    if (scalar(@attrs)) {
        print F "\n// Attributes\n";
        for my $name (@attrs) {
            print F "DEFINE_GLOBAL(QualifiedName, ", $name, "Attr, nullAtom, \"$name\", ${lowerNamespace}NamespaceURI);\n";
        }
        print F "\n\nWebCore::QualifiedName** get${namespace}Attrs(size_t* size)\n";
        print F "{\n    static WebCore::QualifiedName* ${namespace}Attr[] = {\n";
        for my $name (@attrs) {
            print F "        (WebCore::QualifiedName*)&${name}Attr,\n";
        }
        print F "    };\n";
        print F "    *size = ", scalar(@attrs), ";\n";
        print F "    return ${namespace}Attr;\n";
        print F "}\n";
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
    
    print(F "    AtomicString ${lowerNamespace}NS(\"$namespaceURI\");\n\n");

    print(F "    // Namespace\n");
    print(F "    new ((void*)&${lowerNamespace}NamespaceURI) AtomicString(${lowerNamespace}NS);\n\n");
    if (scalar(@tags)) {
        my $tagsNamespace = $tagsNullNamespace ? "nullAtom" : "${lowerNamespace}NS";
        printDefinitions($F, \@tags, "tags", $tagsNamespace);
    }
    if (scalar(@attrs)) {
        my $attrsNamespace = $attrsNullNamespace ? "nullAtom" : "${lowerNamespace}NS";
        printDefinitions($F, \@attrs, "attributes", $attrsNamespace);
    }

    print F "}\n\n} }\n\n";
    close F;
}

sub printElementIncludes
{
    my ($F, @names) = @_;
    for my $name (@names) {
        my $upperCase = upperCaseName($name);
        print F "#include \"${namespace}${upperCase}Element.h\"\n";
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

    for my $name (@$namesRef) {
        print F "    const char *$name","${shortCamelType}String = \"$name\";\n";
    }
        
    for my $name (@$namesRef) {
        if ($name =~ /_/) {
            my $realName = $name;
            $realName =~ s/_/-/g;
            print F "    ${name}${shortCamelType}String = \"$realName\";\n";
        }
    }
    print F "\n";

    for my $name (@$namesRef) {
        print F "    new ((void*)&$name","${shortCamelType}) QualifiedName(nullAtom, $name","${shortCamelType}String, $namespaceURI);\n";
    }

}

sub printFactoryCppFile
{
    my $cppPath = shift;
    my $F;
    open F, ">$cppPath";

printLicenseHeader($F);

print F <<END
#include "config.h"
#include "${namespace}ElementFactory.h"
#include "${namespace}Names.h"
#include "Page.h"
#include "Settings.h"
END
;

printElementIncludes($F, @tags);

print F <<END
#include <wtf/HashMap.h>

using namespace WebCore;
using namespace ${cppNamespace}::${namespace}Names;

typedef ${namespace}Element *(*ConstructorFunc)(Document *doc, bool createdByParser);
typedef WTF::HashMap<AtomicStringImpl*, ConstructorFunc> FunctionMap;

static FunctionMap *gFunctionMap = 0;

namespace ${cppNamespace} {

END
;

printConstructors($F, @tags);

print F <<END
#if ENABLE(SVG)
static inline void createFunctionMapIfNecessary()
{
    if (gFunctionMap)
        return;
    // Create the table.
    gFunctionMap = new FunctionMap;
    
    // Populate it with constructor functions.
END
;

printFunctionInits($F, @tags);

print F <<END
}
#endif

${namespace}Element *${namespace}ElementFactory::create${namespace}Element(const QualifiedName& qName, Document* doc, bool createdByParser)
{
#if ENABLE(SVG)
    // Don't make elements without a document
    if (!doc)
        return 0;
    
    Settings* settings = doc->settings();
    if (settings && settings->usesDashboardBackwardCompatibilityMode())
        return 0;

    createFunctionMapIfNecessary();
    ConstructorFunc func = gFunctionMap->get(qName.localName().impl());
    if (func)
        return func(doc, createdByParser);
    
    return new ${namespace}Element(qName, doc);
#else
    return 0;
#endif
}

} // namespace

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

print F "#ifndef ${namespace}ELEMENTFACTORY_H\n";
print F "#define ${namespace}ELEMENTFACTORY_H\n\n";

print F "
namespace WebCore {
    class Element;
    class Document;
    class QualifiedName;
    class AtomicString;
}

namespace ${cppNamespace}
{
    class ${namespace}Element;

    // The idea behind this class is that there will eventually be a mapping from namespace URIs to ElementFactories that can dispense
    // elements.  In a compound document world, the generic createElement function (will end up being virtual) will be called.
    class ${namespace}ElementFactory
    {
    public:
        WebCore::Element *createElement(const WebCore::QualifiedName& qName, WebCore::Document *doc, bool createdByParser = true);
        static ${namespace}Element *create${namespace}Element(const WebCore::QualifiedName& qName, WebCore::Document *doc, bool createdByParser = true);
    };
}

#endif

";

    close F;
}
