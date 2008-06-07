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

use Config;
use Getopt::Long;
use File::Path;
use IO::File;
use Switch;
use XMLTiny qw(parsefile);

my $printFactory = 0;
my $printWrapperFactory = 0;
my $cppNamespace = "";
my $namespace = "";
my $namespacePrefix = "";
my $namespaceURI = "";
my $tagsFile = "";
my $attrsFile = "";
my $outputDir = ".";
my %tags = ();
my %attrs = ();
my $tagsNullNamespace = 0;
my $attrsNullNamespace = 0;
my $extraDefines = 0;
my $preprocessor = "/usr/bin/gcc -E -P -x c++";
my $guardFactoryWith = 0;
my %htmlCapitalizationHacks = ();
my %svgCustomMappings = ();
my %htmlCustomMappings = ();

GetOptions('tags=s' => \$tagsFile, 
    'attrs=s' => \$attrsFile,
    'outputDir=s' => \$outputDir,
    'namespace=s' => \$namespace,
    'namespacePrefix=s' => \$namespacePrefix,
    'namespaceURI=s' => \$namespaceURI,
    'cppNamespace=s' => \$cppNamespace,
    'factory' => \$printFactory,
    'wrapperFactory' => \$printWrapperFactory,
    'tagsNullNamespace' => \$tagsNullNamespace,
    'attrsNullNamespace' => \$attrsNullNamespace,
    'extraDefines=s' => \$extraDefines,
    'preprocessor=s' => \$preprocessor,
    'guardFactoryWith=s' => \$guardFactoryWith);

die "You must specify a namespace (e.g. SVG) for <namespace>Names.h" unless $namespace;
die "You must specify a namespaceURI (e.g. http://www.w3.org/2000/svg)" unless $namespaceURI;
die "You must specify a cppNamespace (e.g. DOM) used for <cppNamespace>::<namespace>Names::fooTag" unless $cppNamespace;
die "You must specify at least one of --tags <file> or --attrs <file>" unless (length($tagsFile) || length($attrsFile));

$namespacePrefix = $namespace unless $namespacePrefix;

readNames($tagsFile) if length($tagsFile);
readNames($attrsFile) if length($attrsFile);

mkpath($outputDir);
my $namesBasePath = "$outputDir/${namespace}Names";
my $factoryBasePath = "$outputDir/${namespace}ElementFactory";
my $wrapperFactoryBasePath = "$outputDir/JS${namespace}ElementWrapperFactory";

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

### Parsing handlers

sub parseTags
{
    my $contentsRef = shift;
    foreach my $contentRef (@$contentsRef) {
        my $tag = $${contentRef}{'name'};
        $tag =~ s/-/_/g;

        # FIXME: we currently insert '' into the hash but it should be replaced by a description map
        # taking into account the element's attributes.
        $tags{$tag} = '';
    }
}

sub parseAttrs
{
    my $contentsRef = shift;
    foreach my $contentRef (@$contentsRef) {
        my $attr = $${contentRef}{'name'};
        $attr =~ s/-/_/g;

        # FIXME: we currently insert '' into the hash but it should be replaced by a description map
        # taking into account the element's attributes.
        $attrs{$attr} = '';
    }
}

## Support routines

sub readNames
{
    my $namesFile = shift;

    my $names = new IO::File;
    if ($extraDefines eq 0) {
        open($names, $preprocessor . " " . $namesFile . "|") or die "Failed to open file: $namesFile";
    } else {
        open($names, $preprocessor . " -D" . join(" -D", split(" ", $extraDefines)) . " " . $namesFile . "|") or die "Failed to open file: $namesFile";
    }

    # Store hashes keys count to know if some insertion occured.
    my $tagsCount = keys %tags;
    my $attrsCount = keys %attrs;

    my $documentRef = parsefile($names);

    # XML::Tiny returns an array reference to a hash containing the different properties
    my %document = %{@$documentRef[0]};
    my $name = $document{'name'};

    # Check root element to determine what we are parsing
    switch($name) {
        case "tags" {
            parseTags(\@{$document{'content'}});
        }
        case "attrs" {
            parseAttrs(\@{$document{'content'}});
        }
    }

    # FIXME: we should process the attributes here to build a parameter map

    close($names);

    die "Failed to read names from file: $namesFile" if ((keys %tags == $tagsCount) && (keys %attrs == $attrsCount));
}

sub printMacros
{
    my ($F, $macro, $suffix, $namesRef) = @_;
    for my $name (sort keys %$namesRef) {
        print F "    $macro $name","$suffix;\n";
    }
}

sub printConstructors
{
    my ($F, $namesRef) = @_;
    print F "#if $guardFactoryWith\n" if $guardFactoryWith;
    for my $name (sort keys %$namesRef) {
        my $ucName = upperCaseName($name);
    
        print F "${namespace}Element* ${name}Constructor(Document* doc, bool createdByParser)\n";
        print F "{\n";
        print F "    return new ${namespace}${ucName}Element(${name}Tag, doc);\n";
        print F "}\n\n";
    }
    print F "#endif\n" if $guardFactoryWith;
}

sub printFunctionInits
{
    my ($F, $namesRef) = @_;
    for my $name (sort keys %$namesRef) {
        print F "    gFunctionMap->set(${name}Tag.localName().impl(), ${name}Constructor);\n";
    }
}

sub initializeHtmlHacks
{
    if (!keys %htmlCapitalizationHacks) {
        %htmlCapitalizationHacks = ('a' => 'Anchor',
                                    'basefont' => 'BaseFont',
                                    'br' => 'BR',
                                    'caption' => 'TableCaption',
                                    'col' => 'TableCol',
                                    'del' => 'Mod',
                                    'dir' => 'Directory',
                                    'dl' => 'DList',
                                    'fieldset' => 'FieldSet',
                                    'frameset' => 'FrameSet',
                                    'h1' => 'Heading',
                                    'hr' => 'HR',
                                    'iframe' => 'IFrame',
                                    'img' => 'Image',
                                    'isindex' => 'IsIndex',
                                    'li' => 'LI',
                                    'ol' => 'OList',
                                    'optgroup' => 'OptGroup',
                                    'p' => 'Paragraph',
                                    'q' => 'Quote',
                                    'tbody' => 'TableSection',
                                    'td' => 'TableCell',
                                    'textarea' => 'TextArea',
                                    'tr' => 'TableRow',
                                    'ul' => 'UList');
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

    # Process special mapping
    if ($namespace eq "HTML") {
        initializeHtmlHacks();
        return $htmlCapitalizationHacks{$name} if exists $htmlCapitalizationHacks{$name};
    }

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

    if (keys %tags) {
        print F "// Tags\n";
        printMacros($F, "extern const WebCore::QualifiedName", "Tag", \%tags);
        print F "\n\nWebCore::QualifiedName** get${namespace}Tags(size_t* size);\n";
    }
    
    if (keys %attrs) {
        print F "// Attributes\n";
        printMacros($F, "extern const WebCore::QualifiedName", "Attr", \%attrs);
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

    if (keys %tags) {
        print F "// Tags\n";
        for my $name (sort keys %tags) {
            print F "DEFINE_GLOBAL(QualifiedName, ", $name, "Tag, nullAtom, \"$name\", ${lowerNamespace}NamespaceURI);\n";
        }
        
        print F "\n\nWebCore::QualifiedName** get${namespace}Tags(size_t* size)\n";
        print F "{\n    static WebCore::QualifiedName* ${namespace}Tags[] = {\n";
        for my $name (sort keys %tags) {
            print F "        (WebCore::QualifiedName*)&${name}Tag,\n";
        }
        print F "    };\n";
        print F "    *size = ", scalar(keys %tags), ";\n";
        print F "    return ${namespace}Tags;\n";
        print F "}\n";
        
    }

    if (keys %attrs) {
        print F "\n// Attributes\n";
        for my $name (sort keys %attrs) {
            print F "DEFINE_GLOBAL(QualifiedName, ", $name, "Attr, nullAtom, \"$name\", ${lowerNamespace}NamespaceURI);\n";
        }
        print F "\n\nWebCore::QualifiedName** get${namespace}Attrs(size_t* size)\n";
        print F "{\n    static WebCore::QualifiedName* ${namespace}Attr[] = {\n";
        for my $name (sort keys %attrs) {
            print F "        (WebCore::QualifiedName*)&${name}Attr,\n";
        }
        print F "    };\n";
        print F "    *size = ", scalar(keys %attrs), ";\n";
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
    if (keys %tags) {
        my $tagsNamespace = $tagsNullNamespace ? "nullAtom" : "${lowerNamespace}NS";
        printDefinitions($F, \%tags, "tags", $tagsNamespace);
    }
    if (keys %attrs) {
        my $attrsNamespace = $attrsNullNamespace ? "nullAtom" : "${lowerNamespace}NS";
        printDefinitions($F, \%attrs, "attributes", $attrsNamespace);
    }

    print F "}\n\n} }\n\n";
    close F;
}

sub printJSElementIncludes
{
    my ($F, $namesRef) = @_;
    for my $name (sort keys %$namesRef) {
        next if (hasCustomMapping($name));

        my $ucName = upperCaseName($name);
        print F "#include \"JS${namespace}${ucName}Element.h\"\n";
    }
}

sub printElementIncludes
{
    my ($F, $namesRef, $shouldSkipCustomMappings) = @_;
    for my $name (sort keys %$namesRef) {
        next if ($shouldSkipCustomMappings && hasCustomMapping($name));

        my $ucName = upperCaseName($name);
        print F "#include \"${namespace}${ucName}Element.h\"\n";
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

    for my $name (sort keys %$namesRef) {
        print F "    const char *$name","${shortCamelType}String = \"$name\";\n";
    }
        
    for my $name (sort keys %$namesRef) {
        if ($name =~ /_/) {
            my $realName = $name;
            $realName =~ s/_/-/g;
            print F "    ${name}${shortCamelType}String = \"$realName\";\n";
        }
    }
    print F "\n";

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
#include "${namespace}ElementFactory.h"
#include "${namespace}Names.h"
#include "Page.h"
#include "Settings.h"
END
;

printElementIncludes($F, \%tags, 0);

print F <<END
#include <wtf/HashMap.h>

using namespace WebCore;
using namespace ${cppNamespace}::${namespace}Names;

typedef ${namespace}Element* (*ConstructorFunc)(Document* doc, bool createdByParser);
typedef WTF::HashMap<AtomicStringImpl*, ConstructorFunc> FunctionMap;

static FunctionMap* gFunctionMap = 0;

namespace ${cppNamespace} {

END
;

printConstructors($F, \%tags);

print F "#if $guardFactoryWith\n" if $guardFactoryWith;

print F <<END
static inline void createFunctionMapIfNecessary()
{
    if (gFunctionMap)
        return;
    // Create the table.
    gFunctionMap = new FunctionMap;
    
    // Populate it with constructor functions.
END
;

printFunctionInits($F, \%tags);

print F "}\n";
print F "#endif\n\n" if $guardFactoryWith;

print F <<END
${namespace}Element* ${namespace}ElementFactory::create${namespace}Element(const QualifiedName& qName, Document* doc, bool createdByParser)
{
END
;

print F "#if $guardFactoryWith\n" if $guardFactoryWith;

print F <<END
    // Don't make elements without a document
    if (!doc)
        return 0;

#if ENABLE(DASHBOARD_SUPPORT)
    Settings* settings = doc->settings();
    if (settings && settings->usesDashboardBackwardCompatibilityMode())
        return 0;
#endif

    createFunctionMapIfNecessary();
    ConstructorFunc func = gFunctionMap->get(qName.localName().impl());
    if (func)
        return func(doc, createdByParser);

    return new ${namespace}Element(qName, doc);
END
;

if ($guardFactoryWith) {

print F <<END
#else
    return 0;
#endif
END
;

}

print F <<END
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
        WebCore::Element* createElement(const WebCore::QualifiedName& qName, WebCore::Document* doc, bool createdByParser = true);
        static ${namespace}Element* create${namespace}Element(const WebCore::QualifiedName& qName, WebCore::Document* doc, bool createdByParser = true);
    };
}

#endif

";

    close F;
}

## Wrapper Factory routines

sub isMediaTag
{
    my $name = shift;
    return $name eq "audio" || $name eq "source" || $name eq "video" || 0;
}

sub initializeCustomMappings
{
    if (!keys %svgCustomMappings) {
        # These are used to map a tag to another one in WrapperFactory
        # (for example, "h2" is mapped to "h1" so that they use the same JS Wrapper ("h1" wrapper))
        # Mapping to an empty string will not generate a wrapper
        %svgCustomMappings = ('animateMotion' => '',
                              'hkern' => '',
                              'mpath' => '');
        %htmlCustomMappings = ('abbr' => '',
                               'acronym' => '',
                               'address' => '',
                               'b' => '',
                               'bdo' => '',
                               'big' => '',
                               'center' => '',
                               'cite' => '',
                               'code' => '',
                               'colgroup' => 'col',
                               'dd' => '',
                               'dfn' => '',
                               'dt' => '',
                               'em' => '',
                               'h2' => 'h1',
                               'h3' => 'h1',
                               'h4' => 'h1',
                               'h5' => 'h1',
                               'h6' => 'h1',
                               'i' => '',
                               'image' => 'img',
                               'ins' => 'del',
                               'kbd' => '',
                               'keygen' => 'select',
                               'listing' => 'pre',
                               'layer' => '',
                               'nobr' => '',
                               'noembed' => '',
                               'noframes' => '',
                               'nolayer' => '',
                               'noscript' => '',
                               'plaintext' => '',
                               's' => '',
                               'samp' => '',
                               'small' => '',
                               'span' => '',
                               'strike' => '',
                               'strong' => '',
                               'sub' => '',
                               'sup' => '',
                               'tfoot' => 'tbody',
                               'th' => 'td',
                               'thead' => 'tbody',
                               'tt' => '',
                               'u' => '',
                               'var' => '',
                               'wbr' => '',
                               'xmp' => 'pre');
    }
}

sub hasCustomMapping
{
    my $name = shift;
    initializeCustomMappings();
    return 1 if $namespace eq "HTML" && exists($htmlCustomMappings{$name});
    return 1 if $namespace eq "SVG" && exists($svgCustomMappings{$name});
    return 0;
}

sub printWrapperFunctions
{
    my ($F, $namesRef) = @_;
    for my $name (sort keys %$namesRef) {
        # Custom mapping do not need a JS wrapper
        next if (hasCustomMapping($name));

        my $ucName = upperCaseName($name);
        # Hack for the media tags
        if (isMediaTag($name)) {
            print F <<END
static JSNode* create${ucName}Wrapper(ExecState* exec, PassRefPtr<${namespace}Element> element)
{
    if (!MediaPlayer::isAvailable())
        return new JS${namespace}Element(JS${namespace}ElementPrototype::self(exec), element.get());
    return new JS${namespace}${ucName}Element(JS${namespace}${ucName}ElementPrototype::self(exec), static_cast<${namespace}${ucName}Element*>(element.get()));
}

END
;
        } else {
            print F <<END
static JSNode* create${ucName}Wrapper(ExecState* exec, PassRefPtr<${namespace}Element> element)
{   
    return new JS${namespace}${ucName}Element(JS${namespace}${ucName}ElementPrototype::self(exec), static_cast<${namespace}${ucName}Element*>(element.get()));
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

    print F "#if $guardFactoryWith\n\n" if $guardFactoryWith;

    print F "#include \"JS${namespace}ElementWrapperFactory.h\"\n";

    printJSElementIncludes($F, \%tags);

    print F "\n#include \"${namespace}Names.h\"\n\n";

    printElementIncludes($F, \%tags, 1);

    print F <<END
using namespace KJS;

namespace WebCore {

using namespace ${namespace}Names;

typedef JSNode* (*Create${namespace}ElementWrapperFunction)(ExecState*, PassRefPtr<${namespace}Element>);

END
;

    printWrapperFunctions($F, \%tags);

    print F <<END
JSNode* createJS${namespace}Wrapper(ExecState* exec, PassRefPtr<${namespace}Element> element)
{   
    static HashMap<WebCore::AtomicStringImpl*, Create${namespace}ElementWrapperFunction> map;
    if (map.isEmpty()) {
END
;

    for my $tag (sort keys %tags) {
        next if (hasCustomMapping($tag));

        my $ucTag = upperCaseName($tag);
        print F "       map.set(${tag}Tag.localName().impl(), create${ucTag}Wrapper);\n";
    }

    if ($namespace eq "HTML") {
        for my $tag (sort keys %htmlCustomMappings) {
            next if !$htmlCustomMappings{$tag};
            my $ucCustomTag = upperCaseName($htmlCustomMappings{$tag});
            print F "       map.set(${tag}Tag.localName().impl(), create${ucCustomTag}Wrapper);\n";
        }
    }

    # Currently SVG has no need to add custom map.set as it only has empty elements

    print F <<END
    }
    Create${namespace}ElementWrapperFunction createWrapperFunction = map.get(element->localName().impl());
    if (createWrapperFunction)
        return createWrapperFunction(exec, element);
    return new JS${namespace}Element(JS${namespace}ElementPrototype::self(exec), element.get());
}

}

END
;

    print F "#endif\n" if $guardFactoryWith;

    close F;
}

sub printWrapperFactoryHeaderFile
{
    my $headerPath = shift;
    my $F;
    open F, ">$headerPath";

    printLicenseHeader($F);

    print F "#ifndef JS${namespace}ElementWrapperFactory_h\n";
    print F "#define JS${namespace}ElementWrapperFactory_h\n\n";

    print F "#if ${guardFactoryWith}\n" if $guardFactoryWith;

    print F <<END
#include <wtf/Forward.h>

namespace KJS {
    class ExecState;
}                                            
                                             
namespace WebCore {

    class JSNode;
    class ${namespace}Element;

    JSNode* createJS${namespace}Wrapper(KJS::ExecState*, PassRefPtr<${namespace}Element>);

}
 
END
;

    print F "#endif // $guardFactoryWith\n\n" if $guardFactoryWith;

    print F "#endif // JS${namespace}ElementWrapperFactory_h\n";

    close F;
}
