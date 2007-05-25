#!/usr/bin/perl -w

# Copyright (C) 2005, 2006, 2007 Apple, Inc.  All rights reserved.
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
    'extraDefines=s' => \$extraDefines);

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
        die "Failed to open file: $namesFile" unless open NAMES, "-|", "/usr/bin/gcc", "-E", "-P", "-x", "c++", $namesFile or die;
    } else {
        die "Failed to open file: $namesFile" unless open NAMES, "-|", "/usr/bin/gcc", "-E", "-P", "-x", "c++", "-D", "$extraDefines", $namesFile or die;
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
    my ($macro, $suffix, @names) = @_;
    for my $name (@names) {
        print "    $macro $name","$suffix;\n";
    }
}

sub printConstructors
{
    my @names = @_;
    print "#if ENABLE(SVG)\n";
    for my $name (@names) {
        my $upperCase = upperCaseName($name);
    
        print "${namespace}Element *${name}Constructor(Document *doc, bool createdByParser)\n";
        print "{\n";
        print "    return new ${namespace}${upperCase}Element(${name}Tag, doc);\n";
        print "}\n\n";
    }
    print "#endif\n";
}

sub printFunctionInits
{
    my @names = @_;
    for my $name (@names) {
        print "    gFunctionMap->set(${name}Tag.localName().impl(), ${name}Constructor);\n";
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
    $name =~ s/uri/URI/;
    
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
    print "/*
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
    my $headerPath = shift;
    redirectSTDOUT($headerPath);
    
    printLicenseHeader();
    print "#ifndef DOM_${namespace}NAMES_H\n";
    print "#define DOM_${namespace}NAMES_H\n\n";
    print "#include \"QualifiedName.h\"\n\n";
    
    print "namespace $cppNamespace { namespace ${namespace}Names {\n\n";
    
    my $lowerNamespace = lc($namespacePrefix);
    print "#ifndef DOM_${namespace}NAMES_HIDE_GLOBALS\n";
    print "// Namespace\n";
    print "extern const WebCore::AtomicString ${lowerNamespace}NamespaceURI;\n\n";

    if (scalar(@tags)) {
        print "// Tags\n";
        printMacros("extern const WebCore::QualifiedName", "Tag", @tags);
        print "\n\nWebCore::QualifiedName** get${namespace}Tags(size_t* size);\n";
    }
    
    if (scalar(@attrs)) {
        print "// Attributes\n";
        printMacros("extern const WebCore::QualifiedName", "Attr", @attrs);
        print "\n\nWebCore::QualifiedName** get${namespace}Attr(size_t* size);\n";
    }
    print "#endif\n\n";
    print "void init();\n\n";
    print "} }\n\n";
    print "#endif\n\n";
    
    restoreSTDOUT();
}

sub printNamesCppFile
{
    my $cppPath = shift;
    redirectSTDOUT($cppPath);
    
    printLicenseHeader();
    
    my $lowerNamespace = lc($namespacePrefix);

print "#include \"config.h\"\n";

print "#ifdef AVOID_STATIC_CONSTRUCTORS\n";
print "#define DOM_${namespace}NAMES_HIDE_GLOBALS 1\n";
print "#else\n";
print "#define QNAME_DEFAULT_CONSTRUCTOR 1\n";
print "#endif\n\n";


print "#include \"${namespace}Names.h\"\n\n";
print "#include \"StaticConstructors.h\"\n";

print "namespace $cppNamespace { namespace ${namespace}Names {

using namespace WebCore;

DEFINE_GLOBAL(AtomicString, ${lowerNamespace}NamespaceURI, \"$namespaceURI\")
";

    if (scalar(@tags)) {
        print "// Tags\n";
        for my $name (@tags) {
            print "DEFINE_GLOBAL(QualifiedName, ", $name, "Tag, nullAtom, \"$name\", ${lowerNamespace}NamespaceURI);\n";
        }
        
        print "\n\nWebCore::QualifiedName** get${namespace}Tags(size_t* size)\n";
        print "{\n    static WebCore::QualifiedName* ${namespace}Tags[] = {\n";
        for my $name (@tags) {
            print "        (WebCore::QualifiedName*)&${name}Tag,\n";
        }
        print "    };\n";
        print "    *size = ", scalar(@tags), ";\n";
        print "    return ${namespace}Tags;\n";
        print "}\n";
        
    }

    if (scalar(@attrs)) {
        print "\n// Attributes\n";
        for my $name (@attrs) {
            print "DEFINE_GLOBAL(QualifiedName, ", $name, "Attr, nullAtom, \"$name\", ${lowerNamespace}NamespaceURI);\n";
        }
        print "\n\nWebCore::QualifiedName** get${namespace}Attrs(size_t* size)\n";
        print "{\n    static WebCore::QualifiedName* ${namespace}Attr[] = {\n";
        for my $name (@attrs) {
            print "        (WebCore::QualifiedName*)&${name}Attr,\n";
        }
        print "    };\n";
        print "    *size = ", scalar(@attrs), ";\n";
        print "    return ${namespace}Attr;\n";
        print "}\n";
    }

print "\nvoid init()
{
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;
    
    // Use placement new to initialize the globals.
    
    AtomicString::init();
";
    
    print("    AtomicString ${lowerNamespace}NS(\"$namespaceURI\");\n\n");

    print("    // Namespace\n");
    print("    new ((void*)&${lowerNamespace}NamespaceURI) AtomicString(${lowerNamespace}NS);\n\n");
    if (scalar(@tags)) {
        my $tagsNamespace = $tagsNullNamespace ? "nullAtom" : "${lowerNamespace}NS";
        printDefinitions(\@tags, "tags", $tagsNamespace);
    }
    if (scalar(@attrs)) {
        my $attrsNamespace = $attrsNullNamespace ? "nullAtom" : "${lowerNamespace}NS";
        printDefinitions(\@attrs, "attributes", $attrsNamespace);
    }

    print "}\n\n} }\n\n";
    restoreSTDOUT();
}

sub printElementIncludes
{
    my @names = @_;
    for my $name (@names) {
        my $upperCase = upperCaseName($name);
        print "#include \"${namespace}${upperCase}Element.h\"\n";
    }
}

sub printDefinitions
{
    my ($namesRef, $type, $namespaceURI) = @_;
    my $singularType = substr($type, 0, -1);
    my $shortType = substr($singularType, 0, 4);
    my $shortCamelType = ucfirst($shortType);
    my $shortUpperType = uc($shortType);
    
    print "    // " . ucfirst($type) . "\n";

    for my $name (@$namesRef) {
        print "    const char *$name","${shortCamelType}String = \"$name\";\n";
    }
        
    for my $name (@$namesRef) {
        if ($name =~ /_/) {
            my $realName = $name;
            $realName =~ s/_/-/g;
            print "    ${name}${shortCamelType}String = \"$realName\";\n";
        }
    }
    print "\n";

    for my $name (@$namesRef) {
        print "    new ((void*)&$name","${shortCamelType}) QualifiedName(nullAtom, $name","${shortCamelType}String, $namespaceURI);\n";
    }

}

my $savedSTDOUT;

sub redirectSTDOUT
{
    my $filepath = shift;
    print "Writing $filepath...\n";
    open $savedSTDOUT, ">&STDOUT" or die "Can't save STDOUT";
    open(STDOUT, ">", $filepath) or die "Failed to open file: $filepath";
}

sub restoreSTDOUT
{
    open STDOUT, ">&", $savedSTDOUT or die "Can't restor STDOUT: \$oldout: $!";
}

sub printFactoryCppFile
{
    my $cppPath = shift;
    redirectSTDOUT($cppPath);

printLicenseHeader();

print <<END
#include "config.h"
#include "${namespace}ElementFactory.h"
#include "${namespace}Names.h"
#include "Page.h"
#include "Settings.h"
END
;

printElementIncludes(@tags);

print <<END
#include <wtf/HashMap.h>

using namespace WebCore;
using namespace ${cppNamespace}::${namespace}Names;

typedef ${namespace}Element *(*ConstructorFunc)(Document *doc, bool createdByParser);
typedef WTF::HashMap<AtomicStringImpl*, ConstructorFunc> FunctionMap;

static FunctionMap *gFunctionMap = 0;

namespace ${cppNamespace} {

END
;

printConstructors(@tags);

print <<END
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

printFunctionInits(@tags);

print <<END
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

    restoreSTDOUT();
}

sub printFactoryHeaderFile
{
    my $headerPath = shift;
    redirectSTDOUT($headerPath);

    printLicenseHeader();

print "#ifndef ${namespace}ELEMENTFACTORY_H\n";
print "#define ${namespace}ELEMENTFACTORY_H\n\n";

print "
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

    restoreSTDOUT();
}
