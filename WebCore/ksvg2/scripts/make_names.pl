#!/usr/bin/perl -w

use strict;
use Getopt::Long;
use File::Path;


my $namesHeader = 0;
my $namesCpp = 0;
my $factoryHeader = 0;
my $factoryCpp = 0;

my $wrapperNamespace = "KSVG";
my $namespace = "SVG";
my $tagsFile = "ksvg2/svg/tagnames.in";
my $attrsFile = "ksvg2/svg/attrnames.in";

my $outputDir = ".";

GetOptions('tags=s' => \$tagsFile, 
    'attrs=s' => \$attrsFile,
    'outputDir=s' => \$outputDir,
    'namespace=s' => \$namespace);

my @tags = readNames($tagsFile);
my @attrs = readNames($attrsFile);
my @elements = elementsForTags(@tags);

mkpath($outputDir);
my $namesBasePath = "$outputDir/${namespace}Names";
my $factoryBasePath = "$outputDir/${namespace}ElementFactory";

printNamesHeaderFile("$namesBasePath.h");
printNamesCppFile("$namesBasePath.cpp");
printFactoryCppFile("$factoryBasePath.cpp");
printFactoryHeaderFile("$factoryBasePath.h");


sub readNames
{
	my $namesFile = shift;
	
	die "Failed to open file: $namesFile" unless open(NAMES, "<", $namesFile);
	my @names = ();
	while (<NAMES>) {
		s/-/_/g;
		chomp $_;
		push @names, $_;
	}	
	close(NAMES);
	
	return @names
}


sub printMacros
{
	my @names = @_;
	for my $name (@names) {
		print "    macro($name) \\\n";
	}
}

sub printConstructors
{
	my @names = @_;
	for my $name (@names) {
		my $upperCase = upperCaseName($name);
	
		print "${namespace}ElementImpl *${name}Constructor(DocumentImpl *doc, bool createdByParser)\n";
		print "{\n";
		print "    return new ${namespace}${upperCase}ElementImpl(${name}Tag, doc);\n";
		print "}\n\n";
	}
}

sub printFunctionInits
{
	my @names = @_;
	for my $name (@names) {
		print "    gFunctionMap->set(${name}Tag.localName().impl(), (void*)&${name}Constructor);\n";
	}
}

sub upperCaseName
{
	my $name = shift;
	
	$name = camelCaseName($name);
	$name =~ s/svg/SVG/;
	
	if ($name =~ /^fe(.+)$/) {
		return "FE" . ucfirst $1;
	}
	return ucfirst $name;
}

sub camelCaseName
{
	my $name = shift;
	$name =~ s/gradient/Gradient/;
	$name =~ s/color/Color/;
	$name =~ s/animate/Animate/;
	$name =~ s/matrix/Matrix/;
	$name =~ s/node/Node/;
	$name =~ s/turb/Turb/;
	$name =~ s/merge/Merge/;
	$name =~ s/gaus/Gaus/;
	$name =~ s/blur/Blur/;
	$name =~ s/span/Span/;
	$name =~ s/path/Path/;
	$name =~ s/image/Image/;
	$name =~ s/comp/Comp/;
	$name =~ s/off/Off/;
	$name =~ s/flood/Flood/;
	$name =~ s/blend/Blend/;
	$name =~ s/trans/Trans/;
	$name =~ s/glyph/Glyph/;
	$name =~ s/item/Item/;
	$name =~ s/face/Face/;
	$name =~ s/uri/URI/;
	$name =~ s/src/Src/;
	$name =~ s/format/Format/;
	$name =~ s/ref/Ref/;
	$name =~ s/profile/Profile/;
	$name =~ s/spot/Spot/;
	$name =~ s/name/Name/;
	$name =~ s/object/Object/;
	$name =~ s/motion/Motion/;
	$name =~ s/motion/Light/;
	$name =~ s/kern/Kern/;
	$name =~ s/map/Map/;
	$name =~ s/func(.)$/"Func". uc $1/e;
	return lcfirst $name;
}

sub elementsForTags
{
	my @names = @_;
	my @filtered = ();
	for (@names) {
		next if /_/;
		next if /font/i;
		next if /glyph/i;
		next if /kern/i;
		next if /motion/i;
		next if /light/i;
		next if /mask/i;
		next if /meta/i;
		next if /mpath/i;
		next if /tref/i;
		next if /textpath/i;
		next if /foreign/i;
		next if /matrix/i;
		next if /map/i;
		next if /morph/i;
		push(@filtered, $_);
	}
	return @filtered;
}


sub printLicenseHeader
{
	print "/*
 * This file is part of the $namespace DOM implementation for KDE.
 *
 * Copyright (C) 2005 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
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
	print "#include \"dom_qname.h\"\n\n";
	
	print "namespace $wrapperNamespace { namespace ${namespace}Names {\n\n";
	
	print"#define DOM_${namespace}NAMES_FOR_EACH_TAG(macro) \\\n";
	printMacros(@tags);
	print"// end of macro\n\n";
	print "#define DOM_${namespace}NAMES_FOR_EACH_ATTR(macro) \\\n";
	printMacros(@attrs);
	print "// end of macro\n\n";
	
	my $lowerNamespace = lc($namespace);
	print"#if !DOM_${namespace}NAMES_HIDE_GLOBALS
    // Namespace
    extern const DOM::AtomicString ${lowerNamespace}NamespaceURI;

    // Tags
    #define DOM_NAMES_DEFINE_TAG_GLOBAL(name) extern const DOM::QualifiedName name##Tag;
    DOM_${namespace}NAMES_FOR_EACH_TAG(DOM_NAMES_DEFINE_TAG_GLOBAL)
    #undef DOM_NAMES_DEFINE_TAG_GLOBAL

    // Attributes
    #define DOM_NAMES_DEFINE_ATTR_GLOBAL(name) extern const DOM::QualifiedName name##Attr;
    DOM_${namespace}NAMES_FOR_EACH_ATTR(DOM_NAMES_DEFINE_ATTR_GLOBAL)
    #undef DOM_NAMES_DEFINE_ATTR_GLOBAL
#endif

    void init();
} }

#endif

";
	restoreSTDOUT();
}

sub printNamesCppFile
{
	my $cppPath = shift;
	redirectSTDOUT($cppPath);
	
	printLicenseHeader();

print "#define DOM_${namespace}NAMES_HIDE_GLOBALS 1\n\n";

print "#include \"config.h\"\n";
print "#include \"${namespace}Names.h\"\n\n";

print "namespace $wrapperNamespace { namespace ${namespace}Names {

using namespace KDOM;

// Define a properly-sized array of pointers to avoid static initialization.
// Use an array of pointers instead of an array of char in case there is some alignment issue.

#define DEFINE_UNINITIALIZED_GLOBAL(type, name) void *name[(sizeof(type) + sizeof(void *) - 1) / sizeof(void *)];

DEFINE_UNINITIALIZED_GLOBAL(DOM::AtomicString, svgNamespaceURI)

#define DEFINE_TAG_GLOBAL(name) DEFINE_UNINITIALIZED_GLOBAL(DOM::QualifiedName, name##Tag)
DOM_${namespace}NAMES_FOR_EACH_TAG(DEFINE_TAG_GLOBAL)

#define DEFINE_ATTR_GLOBAL(name) DEFINE_UNINITIALIZED_GLOBAL(DOM::QualifiedName, name##Attr)
DOM_${namespace}NAMES_FOR_EACH_ATTR(DEFINE_ATTR_GLOBAL)

void init()
{
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;
    
    // Use placement new to initialize the globals.
";

	my $lowerNamespace = lc($namespace);
	
    print("    DOM::AtomicString svgNS(\"http://www.w3.org/2000/svg\");\n\n");

    print("    // Namespace\n");
    print("    new (&${lowerNamespace}NamespaceURI) DOM::AtomicString(${lowerNamespace}NS);\n");
	printDefinitions(\@tags, "tags", "${lowerNamespace}NS");
	printDefinitions(\@attrs, "attributes", "nullAtom");

	print "}\n\n} }";
	restoreSTDOUT();
}

sub printElementIncludes
{
	my @names = @_;
	for my $name (@names) {
		my $upperCase = upperCaseName($name);
		print "#include \"${namespace}${upperCase}ElementImpl.h\"\n";
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
	print "    #define DEFINE_${shortUpperType}_STRING(name) const char *name##${shortCamelType}String = #name;\n";
	print "    DOM_${namespace}NAMES_FOR_EACH_${shortUpperType}(DEFINE_${shortUpperType}_STRING)\n\n";
	for my $name (@$namesRef) {
		if ($name =~ /_/) {
			my $realName = $name;
			$realName =~ s/_/-/;
			print "    ${name}${shortCamelType}String = \"$realName\";\n";
		}
	}
	print "\n    #define INITIALIZE_${shortUpperType}_GLOBAL(name) new (&name##${shortCamelType}) DOM::QualifiedName(nullAtom, name##${shortCamelType}String, $namespaceURI);\n";
	print "    DOM_${namespace}NAMES_FOR_EACH_${shortUpperType}(INITIALIZE_${shortUpperType}_GLOBAL)\n\n";
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

print "#include \"config.h\"\n\n";
print "#include \"SVGElementFactory.h\"\n";
print "#include \"${namespace}Names.h\"\n\n";

printElementIncludes(@elements);

print "\n\n#include <kxmlcore/HashMap.h>\n\n";

print "using namespace KDOM;\n";
print "using namespace ${wrapperNamespace}::${namespace}Names;\n\n";

print "typedef KXMLCore::HashMap<DOMStringImpl *, void *, KXMLCore::PointerHash<DOMStringImpl *> > FunctionMap;\n";
print "static FunctionMap *gFunctionMap = 0;\n\n";

print "namespace ${wrapperNamespace}\n{\n\n";

print "typedef ${namespace}ElementImpl *(*ConstructorFunc)(DocumentImpl *doc, bool createdByParser);\n\n";

printConstructors(@elements);

print "
static inline void createFunctionMapIfNecessary()
{
    if (gFunctionMap)
        return;
    // Create the table.
    gFunctionMap = new FunctionMap;
    
    // Populate it with constructor functions.
";

printFunctionInits(@elements);

print "}\n";

print "
${namespace}ElementImpl *${namespace}ElementFactory::create${namespace}Element(const QualifiedName& qName, DocumentImpl* doc, bool createdByParser)
{
    if (!doc)
        return 0; // Don't allow elements to ever be made without having a doc.

    createFunctionMapIfNecessary();
    void* result = gFunctionMap->get(qName.localName().impl());
    if (result) {
        ConstructorFunc func = (ConstructorFunc)result;
        return (func)(doc, createdByParser);
    }
    
    return new ${namespace}ElementImpl(qName, doc);
}

}; // namespace

";
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
namespace KDOM {
	class ElementImpl;
	class DocumentImpl;
	class QualifiedName;
	class AtomicString;
}

namespace ${wrapperNamespace}
{
	class ${namespace}ElementImpl;
	
	// The idea behind this class is that there will eventually be a mapping from namespace URIs to ElementFactories that can dispense
	// elements.  In a compound document world, the generic createElement function (will end up being virtual) will be called.
	class ${namespace}ElementFactory
	{
	public:
		KDOM::ElementImpl *createElement(const KDOM::QualifiedName& qName, KDOM::DocumentImpl *doc, bool createdByParser = true);
		static ${namespace}ElementImpl *create${namespace}Element(const KDOM::QualifiedName& qName, KDOM::DocumentImpl *doc, bool createdByParser = true);
	};
}

#endif

";

	restoreSTDOUT();
}


