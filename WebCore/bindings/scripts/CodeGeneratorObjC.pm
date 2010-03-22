# 
# Copyright (C) 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
# Copyright (C) 2006 Anders Carlsson <andersca@mac.com> 
# Copyright (C) 2006, 2007 Samuel Weinig <sam@webkit.org>
# Copyright (C) 2006 Alexey Proskuryakov <ap@webkit.org>
# Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
# Copyright (C) 2009 Cameron McCormack <cam@mcc.id.au>
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

package CodeGeneratorObjC;

use File::stat;

# Global Variables
my $module = "";
my $outputDir = "";
my $writeDependencies = 0;
my %publicInterfaces = ();
my $newPublicClass = 0;
my $interfaceAvailabilityVersion = "";
my $isProtocol = 0;
my $noImpl = 0;

my @headerContentHeader = ();
my @headerContent = ();
my %headerForwardDeclarations = ();
my %headerForwardDeclarationsForProtocols = ();

my @privateHeaderContentHeader = ();
my @privateHeaderContent = ();
my %privateHeaderForwardDeclarations = ();
my %privateHeaderForwardDeclarationsForProtocols = ();

my @internalHeaderContent = ();

my @implContentHeader = ();
my @implContent = ();
my %implIncludes = ();
my @depsContent = ();

# Hashes
my %protocolTypeHash = ("XPathNSResolver" => 1, "EventListener" => 1, "EventTarget" => 1, "NodeFilter" => 1,
                        "SVGLocatable" => 1, "SVGTransformable" => 1, "SVGStylable" => 1, "SVGFilterPrimitiveStandardAttributes" => 1, 
                        "SVGTests" => 1, "SVGLangSpace" => 1, "SVGExternalResourcesRequired" => 1, "SVGURIReference" => 1,
                        "SVGZoomAndPan" => 1, "SVGFitToViewBox" => 1, "SVGAnimatedPathData" => 1, "SVGAnimatedPoints" => 1,
                        "ElementTimeControl" => 1);
my %nativeObjCTypeHash = ("URL" => 1, "Color" => 1);

# FIXME: this should be replaced with a function that recurses up the tree
# to find the actual base type.
my %baseTypeHash = ("Object" => 1, "Node" => 1, "NodeList" => 1, "NamedNodeMap" => 1, "DOMImplementation" => 1,
                    "Event" => 1, "CSSRule" => 1, "CSSValue" => 1, "StyleSheet" => 1, "MediaList" => 1,
                    "Counter" => 1, "Rect" => 1, "RGBColor" => 1, "XPathExpression" => 1, "XPathResult" => 1,
                    "NodeIterator" => 1, "TreeWalker" => 1, "AbstractView" => 1, "Blob" => 1,
                    "SVGAngle" => 1, "SVGAnimatedAngle" => 1, "SVGAnimatedBoolean" => 1, "SVGAnimatedEnumeration" => 1,
                    "SVGAnimatedInteger" => 1, "SVGAnimatedLength" => 1, "SVGAnimatedLengthList" => 1,
                    "SVGAnimatedNumber" => 1, "SVGAnimatedNumberList" => 1, "SVGAnimatedPoints" => 1,
                    "SVGAnimatedPreserveAspectRatio" => 1, "SVGAnimatedRect" => 1, "SVGAnimatedString" => 1,
                    "SVGAnimatedTransformList" => 1, "SVGLength" => 1, "SVGLengthList" => 1, "SVGMatrix" => 1,
                    "SVGNumber" => 1, "SVGNumberList" => 1, "SVGPathSeg" => 1, "SVGPathSegList" => 1, "SVGPoint" => 1,
                    "SVGPointList" => 1, "SVGPreserveAspectRatio" => 1, "SVGRect" => 1, "SVGRenderingIntent" => 1,
                    "SVGStringList" => 1, "SVGTransform" => 1, "SVGTransformList" => 1, "SVGUnitTypes" => 1);

# Constants
my $buildingForTigerOrEarlier = 1 if $ENV{"MACOSX_DEPLOYMENT_TARGET"} and $ENV{"MACOSX_DEPLOYMENT_TARGET"} <= 10.4;
my $buildingForLeopardOrLater = 1 if $ENV{"MACOSX_DEPLOYMENT_TARGET"} and $ENV{"MACOSX_DEPLOYMENT_TARGET"} >= 10.5;
my $exceptionInit = "WebCore::ExceptionCode ec = 0;";
my $exceptionRaiseOnError = "WebCore::raiseOnDOMError(ec);";
my $assertMainThread = "{ DOM_ASSERT_MAIN_THREAD(); WebCoreThreadViolationCheckRoundOne(); }";

my %conflictMethod = (
    # FIXME: Add C language keywords?
    # FIXME: Add other predefined types like "id"?

    "callWebScriptMethod:withArguments:" => "WebScriptObject",
    "evaluateWebScript:" => "WebScriptObject",
    "removeWebScriptKey:" => "WebScriptObject",
    "setException:" => "WebScriptObject",
    "setWebScriptValueAtIndex:value:" => "WebScriptObject",
    "stringRepresentation" => "WebScriptObject",
    "webScriptValueAtIndex:" => "WebScriptObject",

    "autorelease" => "NSObject",
    "awakeAfterUsingCoder:" => "NSObject",
    "class" => "NSObject",
    "classForCoder" => "NSObject",
    "conformsToProtocol:" => "NSObject",
    "copy" => "NSObject",
    "copyWithZone:" => "NSObject",
    "dealloc" => "NSObject",
    "description" => "NSObject",
    "doesNotRecognizeSelector:" => "NSObject",
    "encodeWithCoder:" => "NSObject",
    "finalize" => "NSObject",
    "forwardInvocation:" => "NSObject",
    "hash" => "NSObject",
    "init" => "NSObject",
    "initWithCoder:" => "NSObject",
    "isEqual:" => "NSObject",
    "isKindOfClass:" => "NSObject",
    "isMemberOfClass:" => "NSObject",
    "isProxy" => "NSObject",
    "methodForSelector:" => "NSObject",
    "methodSignatureForSelector:" => "NSObject",
    "mutableCopy" => "NSObject",
    "mutableCopyWithZone:" => "NSObject",
    "performSelector:" => "NSObject",
    "release" => "NSObject",
    "replacementObjectForCoder:" => "NSObject",
    "respondsToSelector:" => "NSObject",
    "retain" => "NSObject",
    "retainCount" => "NSObject",
    "self" => "NSObject",
    "superclass" => "NSObject",
    "zone" => "NSObject",
);

my $fatalError = 0;

# Default License Templates
my $headerLicenseTemplate = << "EOF";
/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig\@gmail.com>
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
EOF

my $implementationLicenseTemplate = << "EOF";
/*
 * This file is part of the WebKit open source project.
 * This file has been generated by generate-bindings.pl. DO NOT MODIFY!
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
EOF

# Default constructor
sub new
{
    my $object = shift;
    my $reference = { };

    $codeGenerator = shift;
    $outputDir = shift;
    shift; # $useLayerOnTop
    shift; # $preprocessor
    $writeDependencies = shift;

    bless($reference, $object);
    return $reference;
}

sub finish
{
    my $object = shift;
}

sub ReadPublicInterfaces
{
    my $class = shift;
    my $superClass = shift;
    my $defines = shift;
    my $isProtocol = shift;

    my $found = 0;
    my $actualSuperClass;
    %publicInterfaces = ();

    my $fileName = "WebCore/bindings/objc/PublicDOMInterfaces.h";
    my $gccLocation = "";
    if (($Config::Config{'osname'}) =~ /solaris/i) {
        $gccLocation = "/usr/sfw/bin/gcc";
    } else {
        $gccLocation = "/usr/bin/gcc";
    }
    open FILE, "-|", $gccLocation, "-E", "-P", "-x", "objective-c",
        (map { "-D$_" } split(/ +/, $defines)), "-DOBJC_CODE_GENERATION", $fileName or die "Could not open $fileName";
    my @documentContent = <FILE>;
    close FILE;

    foreach $line (@documentContent) {
        if (!$isProtocol && $line =~ /^\s*\@interface\s*$class\s*:\s*(\w+)\s*([A-Z0-9_]*)/) {
            if ($superClass ne $1) {
                warn "Public API change. Superclass for \"$class\" differs ($1 != $superClass)";
                $fatalError = 1;
            }

            $interfaceAvailabilityVersion = $2 if defined $2;
            $found = 1;
            next;
        } elsif ($isProtocol && $line =~ /^\s*\@protocol $class\s*<[^>]+>\s*([A-Z0-9_]*)/) {
            $interfaceAvailabilityVersion = $1 if defined $1;
            $found = 1;
            next;
        }

        last if $found and $line =~ /^\s?\@end\s?$/;

        if ($found) {
            # trim whitspace
            $line =~ s/^\s+//;
            $line =~ s/\s+$//;

            my $availabilityMacro = "";
            $line =~ s/\s([A-Z0-9_]+)\s*;$/;/;
            $availabilityMacro = $1 if defined $1;

            $publicInterfaces{$line} = $availabilityMacro if length $line;
        }
    }

    # If this class was not found in PublicDOMInterfaces.h then it should be considered as an entirely new public class.
    $newPublicClass = !$found;
    $interfaceAvailabilityVersion = "WEBKIT_VERSION_LATEST" if $newPublicClass;
}

# Params: 'domClass' struct
sub GenerateInterface
{
    my $object = shift;
    my $dataNode = shift;
    my $defines = shift;

    $fatalError = 0;

    my $name = $dataNode->name;
    my $className = GetClassName($name);
    my $parentClassName = "DOM" . GetParentImplClassName($dataNode);
    $isProtocol = $dataNode->extendedAttributes->{ObjCProtocol};
    $noImpl = $dataNode->extendedAttributes->{ObjCCustomImplementation} || $isProtocol;

    ReadPublicInterfaces($className, $parentClassName, $defines, $isProtocol);

    # Start actual generation..
    $object->GenerateHeader($dataNode);
    $object->GenerateImplementation($dataNode) unless $noImpl;

    # Write changes.
    $object->WriteData("DOM" . $name);

    # Check for missing public API
    if (keys %publicInterfaces > 0) {
        my $missing = join("\n", keys %publicInterfaces);
        warn "Public API change. There are missing public properties and/or methods from the \"$className\" class.\n$missing\n";
        $fatalError = 1;
    }

    die if $fatalError;
}

# Params: 'idlDocument' struct
sub GenerateModule
{
    my $object = shift;
    my $dataNode = shift;

    $module = $dataNode->module;
}

sub GetClassName
{
    my $name = $codeGenerator->StripModule(shift);

    # special cases
    return "NSString" if $codeGenerator->IsStringType($name) or $name eq "SerializedScriptValue";
    return "NS$name" if IsNativeObjCType($name);
    return "BOOL" if $name eq "boolean";
    return "unsigned" if $name eq "unsigned long";
    return "int" if $name eq "long";
    return "NSTimeInterval" if $name eq "Date";
    return "DOMAbstractView" if $name eq "DOMWindow";
    return $name if $codeGenerator->IsPrimitiveType($name) or $name eq "DOMImplementation" or $name eq "DOMTimeStamp";

    # Default, assume Objective-C type has the same type name as
    # idl type prefixed with "DOM".
    return "DOM$name";
}

sub GetClassHeaderName
{
    my $name = shift;

    return "DOMDOMImplementation" if $name eq "DOMImplementation";
    return $name;
}

sub GetImplClassName
{
    my $name = $codeGenerator->StripModule(shift);

    return "DOMImplementationFront" if $name eq "DOMImplementation";
    return "DOMWindow" if $name eq "AbstractView";
    return $name;
}

sub GetParentImplClassName
{
    my $dataNode = shift;

    return "Object" if @{$dataNode->parents} eq 0;

    my $parent = $codeGenerator->StripModule($dataNode->parents(0));

    # special cases
    return "Object" if $parent eq "HTMLCollection";

    return $parent;
}

sub GetParentAndProtocols
{
    my $dataNode = shift;
    my $numParents = @{$dataNode->parents};

    my $parent = "";
    my @protocols = ();
    if ($numParents eq 0) {
        if ($isProtocol) {
            push(@protocols, "NSObject");
            push(@protocols, "NSCopying") if $dataNode->name eq "EventTarget";
        } else {
            $parent = "DOMObject";
        }
    } elsif ($numParents eq 1) {
        my $parentName = $codeGenerator->StripModule($dataNode->parents(0));
        if ($isProtocol) {
            die "Parents of protocols must also be protocols." unless IsProtocolType($parentName);
            push(@protocols, "DOM" . $parentName);
        } else {
            if (IsProtocolType($parentName)) {
                push(@protocols, "DOM" . $parentName);
            } elsif ($parentName eq "HTMLCollection") {
                $parent = "DOMObject";
            } else {
                $parent = "DOM" . $parentName;
            }
        }
    } else {
        my @parents = @{$dataNode->parents};
        my $firstParent = $codeGenerator->StripModule(shift(@parents));
        if (IsProtocolType($firstParent)) {
            push(@protocols, "DOM" . $firstParent);
            if (!$isProtocol) {
                $parent = "DOMObject";
            }
        } else {
            $parent = "DOM" . $firstParent;
        }

        foreach my $parentName (@parents) {
            $parentName = $codeGenerator->StripModule($parentName);
            die "Everything past the first class should be a protocol!" unless IsProtocolType($parentName);

            push(@protocols, "DOM" . $parentName);
        }
    }

    return ($parent, @protocols);
}

sub GetBaseClass
{
    $parent = shift;

    return $parent if $parent eq "Object" or IsBaseType($parent);
    return "Event" if $parent eq "UIEvent";
    return "CSSValue" if $parent eq "SVGColor" or $parent eq "CSSValueList";
    return "Node";
}

sub IsBaseType
{
    my $type = shift;

    return 1 if $baseTypeHash{$type};
    return 0;
}

sub IsProtocolType
{
    my $type = shift;

    return 1 if $protocolTypeHash{$type};
    return 0;
}

sub IsNativeObjCType
{
    my $type = shift;

    return 1 if $nativeObjCTypeHash{$type};
    return 0;
}

sub GetObjCType
{
    my $type = shift;
    my $name = GetClassName($type);

    return "id <$name>" if IsProtocolType($type);
    return $name if $codeGenerator->IsPrimitiveType($type) or $type eq "DOMTimeStamp";
    return "unsigned short" if $type eq "CompareHow" or $type eq "SVGPaintType";
    return "$name *";
}

sub GetPropertyAttributes
{
    my $type = $codeGenerator->StripModule(shift);
    my $readOnly = shift;

    my @attributes = ();

    push(@attributes, "readonly") if $readOnly;

    # FIXME: <rdar://problem/5049934> Consider using 'nonatomic' on the DOM @property declarations.
    if ($codeGenerator->IsStringType($type) || IsNativeObjCType($type)) {
        push(@attributes, "copy");
    } elsif ($codeGenerator->IsPodType($type) || $codeGenerator->IsSVGAnimatedType($type)) {
        push(@attributes, "retain");
    } elsif (!$codeGenerator->IsStringType($type) && !$codeGenerator->IsPrimitiveType($type) && $type ne "DOMTimeStamp" && $type ne "CompareHow" && $type ne "SVGPaintType") {
        push(@attributes, "retain");
    }

    return "" unless @attributes > 0;
    return "(" . join(", ", @attributes) . ")";
}

sub ConversionNeeded
{
    my $type = $codeGenerator->StripModule(shift);

    return !$codeGenerator->IsNonPointerType($type) && !$codeGenerator->IsStringType($type) && !IsNativeObjCType($type);
}

sub GetObjCTypeGetter
{
    my $argName = shift;
    my $type = $codeGenerator->StripModule(shift);

    return $argName if $codeGenerator->IsPrimitiveType($type) or $codeGenerator->IsStringType($type) or IsNativeObjCType($type);
    return $argName . "Node" if $type eq "EventTarget";
    return "static_cast<WebCore::Range::CompareHow>($argName)" if $type eq "CompareHow";
    return "static_cast<WebCore::SVGPaint::SVGPaintType>($argName)" if $type eq "SVGPaintType";
    return "WTF::getPtr(nativeEventListener)" if $type eq "EventListener";
    return "WTF::getPtr(nativeNodeFilter)" if $type eq "NodeFilter";
    return "WTF::getPtr(nativeResolver)" if $type eq "XPathNSResolver";
    
    if ($type eq "SerializedScriptValue") {
        $implIncludes{"SerializedScriptValue.h"} = 1;
        return "WebCore::SerializedScriptValue::create(WebCore::String($argName))";
    }
    return "core($argName)";
}

sub AddForwardDeclarationsForType
{
    my $type = $codeGenerator->StripModule(shift);
    my $public = shift;

    return if $codeGenerator->IsNonPointerType($type) ;

    my $class = GetClassName($type);

    if (IsProtocolType($type)) {
        $headerForwardDeclarationsForProtocols{$class} = 1 if $public;
        $privateHeaderForwardDeclarationsForProtocols{$class} = 1 if !$public and !$headerForwardDeclarationsForProtocols{$class};
        return;
    }

    $headerForwardDeclarations{$class} = 1 if $public;

    # Private headers include the public header, so only add a forward declaration to the private header
    # if the public header does not already have the same forward declaration.
    $privateHeaderForwardDeclarations{$class} = 1 if !$public and !$headerForwardDeclarations{$class};
}

sub AddIncludesForType
{
    my $type = $codeGenerator->StripModule(shift);

    return if $codeGenerator->IsNonPointerType($type);

    if (IsNativeObjCType($type)) {
        if ($type eq "Color") {
            $implIncludes{"ColorMac.h"} = 1;
        }
        return;
    }

    if ($codeGenerator->IsStringType($type)) {
        $implIncludes{"KURL.h"} = 1;
        return;
    }

    if ($type eq "DOMWindow") {
        $implIncludes{"DOMAbstractViewInternal.h"} = 1;
        $implIncludes{"DOMWindow.h"} = 1;
        return;
    }

    if ($type eq "DOMImplementation") {
        $implIncludes{"DOMDOMImplementationInternal.h"} = 1;
        $implIncludes{"DOMImplementationFront.h"} = 1;
        return;
    }

    if ($type eq "EventTarget") {
        $implIncludes{"Node.h"} = 1;
        $implIncludes{"DOMEventTarget.h"} = 1;
        return;
    }

    if ($codeGenerator->IsSVGAnimatedType($type)) {
        $implIncludes{"SVGAnimatedTemplate.h"} = 1;
        $implIncludes{"DOM${type}Internal.h"} = 1;
        return;
    }

    if ($type eq "SVGRect") {
        $implIncludes{"FloatRect.h"} = 1;
        $implIncludes{"DOMSVGRectInternal.h"} = 1;
        return;
    }

    if ($type eq "SVGPoint") {
        $implIncludes{"FloatPoint.h"} = 1;
        $implIncludes{"DOMSVGPointInternal.h"} = 1;
        return;
    }

    if ($type eq "SVGMatrix") {
        $implIncludes{"AffineTransform.h"} = 1;
        $implIncludes{"DOMSVGMatrixInternal.h"} = 1;
        $implIncludes{"SVGException.h"} = 1;
        return;
    }

    if ($type eq "SVGNumber") {
        $implIncludes{"DOMSVGNumberInternal.h"} = 1;
        return;
    }

    if ($type =~ /(\w+)(Abs|Rel)$/) {
        $implIncludes{"$1.h"} = 1;
        $implIncludes{"DOM${type}Internal.h"} = 1;
        return;
    }

    if ($type eq "NodeFilter") {
        $implIncludes{"NodeFilter.h"} = 1;
        $implIncludes{"ObjCNodeFilterCondition.h"} = 1;
        return;
    }

    if ($type eq "EventListener") {
        $implIncludes{"EventListener.h"} = 1;
        $implIncludes{"ObjCEventListener.h"} = 1;
        return;
    }

    if ($type eq "XPathNSResolver") {
        $implIncludes{"DOMCustomXPathNSResolver.h"} = 1;
        $implIncludes{"XPathNSResolver.h"} = 1;
        return;
    }

    if ($type eq "SerializedScriptValue") {
        $implIncludes{"SerializedScriptValue.h"} = 1;
        return;
    }

    # FIXME: won't compile without these
    $implIncludes{"CSSMutableStyleDeclaration.h"} = 1 if $type eq "CSSStyleDeclaration";
    $implIncludes{"NameNodeList.h"} = 1 if $type eq "NodeList";

    # Default, include the same named file (the implementation) and the same name prefixed with "DOM". 
    $implIncludes{"$type.h"} = 1;
    $implIncludes{"DOM${type}Internal.h"} = 1;
}

sub GenerateHeader
{
    my $object = shift;
    my $dataNode = shift;

    my $interfaceName = $dataNode->name;
    my $className = GetClassName($interfaceName);

    my $parentName = "";
    my @protocolsToImplement = ();
    ($parentName, @protocolsToImplement) = GetParentAndProtocols($dataNode);

    my $numConstants = @{$dataNode->constants};
    my $numAttributes = @{$dataNode->attributes};
    my $numFunctions = @{$dataNode->functions};

    # - Add default header template
    @headerContentHeader = split("\r", $headerLicenseTemplate);
    push(@headerContentHeader, "\n");

    # - INCLUDES -
    my $includedWebKitAvailabilityHeader = 0;
    unless ($isProtocol) {
        my $parentHeaderName = GetClassHeaderName($parentName);
        push(@headerContentHeader, "#import <WebCore/$parentHeaderName.h>\n");
        $includedWebKitAvailabilityHeader = 1;
    }

    foreach my $parentProtocol (@protocolsToImplement) {
        next if $parentProtocol =~ /^NS/; 
        $parentProtocol = GetClassHeaderName($parentProtocol);
        push(@headerContentHeader, "#import <WebCore/$parentProtocol.h>\n");
        $includedWebKitAvailabilityHeader = 1;
    }

    # Special case needed for legacy support of DOMRange
    if ($interfaceName eq "Range") {
        push(@headerContentHeader, "#import <WebCore/DOMCore.h>\n");
        push(@headerContentHeader, "#import <WebCore/DOMDocument.h>\n");
        push(@headerContentHeader, "#import <WebCore/DOMRangeException.h>\n");
        $includedWebKitAvailabilityHeader = 1;
    }

    push(@headerContentHeader, "#import <JavaScriptCore/WebKitAvailability.h>\n") unless $includedWebKitAvailabilityHeader;

    my $interfaceAvailabilityVersionCheck = "#if WEBKIT_VERSION_MAX_ALLOWED >= $interfaceAvailabilityVersion\n\n";

    push(@headerContentHeader, "\n");
    push(@headerContentHeader, $interfaceAvailabilityVersionCheck) if length $interfaceAvailabilityVersion;

    # - Add constants.
    if ($numConstants > 0) {
        my @headerConstants = ();

        # FIXME: we need a way to include multiple enums.
        foreach my $constant (@{$dataNode->constants}) {
            my $constantName = $constant->name;
            my $constantValue = $constant->value;

            my $output = "    DOM_" . $constantName . " = " . $constantValue;
            push(@headerConstants, $output);
        }

        my $combinedConstants = join(",\n", @headerConstants);

        # FIXME: the formatting of the enums should line up the equal signs.
        # FIXME: enums are unconditionally placed in the public header.
        push(@headerContent, "enum {\n");
        push(@headerContent, $combinedConstants);
        push(@headerContent, "\n};\n\n");        
    }

    # - Begin @interface or @protocol
    my $interfaceDeclaration = ($isProtocol ? "\@protocol $className" : "\@interface $className : $parentName");
    $interfaceDeclaration .= " <" . join(", ", @protocolsToImplement) . ">" if @protocolsToImplement > 0;
    $interfaceDeclaration .= "\n";

    push(@headerContent, $interfaceDeclaration);

    my @headerAttributes = ();
    my @privateHeaderAttributes = ();

    # - Add attribute getters/setters.
    if ($numAttributes > 0) {
        foreach my $attribute (@{$dataNode->attributes}) {
            my $attributeName = $attribute->signature->name;

            if ($attributeName eq "id" or $attributeName eq "hash") {
                # Special case attributes id and hash to be idName and hashName to avoid ObjC naming conflict.
                $attributeName .= "Name";
            } elsif ($attributeName eq "frame") {
                # Special case attribute frame to be frameBorders.
                $attributeName .= "Borders";
            }

            my $attributeType = GetObjCType($attribute->signature->type);
            my $attributeIsReadonly = ($attribute->type =~ /^readonly/);

            my $property = "\@property" . GetPropertyAttributes($attribute->signature->type, $attributeIsReadonly);
            # Some SVGFE*Element.idl use 'operator' as attribute name, rewrite as '_operator' to avoid clashes with C/C++
            $attributeName =~ s/operator/_operator/ if ($attributeName =~ /operator/);
            $property .= " " . $attributeType . ($attributeType =~ /\*$/ ? "" : " ") . $attributeName;

            my $publicInterfaceKey = $property . ";";

            my $availabilityMacro = "";
            if (defined $publicInterfaces{$publicInterfaceKey} and length $publicInterfaces{$publicInterfaceKey}) {
                $availabilityMacro = $publicInterfaces{$publicInterfaceKey};
            }

            $availabilityMacro = "WEBKIT_OBJC_METHOD_ANNOTATION($availabilityMacro)" if length $availabilityMacro and $buildingForTigerOrEarlier;

            my $declarationSuffix = ";\n";
            $declarationSuffix = " $availabilityMacro;\n" if length $availabilityMacro;

            my $public = (defined $publicInterfaces{$publicInterfaceKey} or $newPublicClass);
            delete $publicInterfaces{$publicInterfaceKey};

            AddForwardDeclarationsForType($attribute->signature->type, $public);

            my $setterName = "set" . ucfirst($attributeName) . ":";

            my $conflict = $conflictMethod{$attributeName};
            if ($conflict) {
                warn "$className conflicts with $conflict method $attributeName\n";
                $fatalError = 1;
            }

            $conflict = $conflictMethod{$setterName};
            if ($conflict) {
                warn "$className conflicts with $conflict method $setterName\n";
                $fatalError = 1;
            }

            if ($buildingForLeopardOrLater) {
                $property .= $declarationSuffix;
                push(@headerAttributes, $property) if $public;
                push(@privateHeaderAttributes, $property) unless $public;
            } else {
                # - GETTER
                my $getter = "- (" . $attributeType . ")" . $attributeName . $declarationSuffix;
                push(@headerAttributes, $getter) if $public;
                push(@privateHeaderAttributes, $getter) unless $public;

                # - SETTER
                if (!$attributeIsReadonly) {
                    my $setter = "- (void)$setterName(" . $attributeType . ")new" . ucfirst($attributeName) . $declarationSuffix;
                    push(@headerAttributes, $setter) if $public;
                    push(@privateHeaderAttributes, $setter) unless $public;
                }
            }
        }

        push(@headerContent, @headerAttributes) if @headerAttributes > 0;
    }

    my @headerFunctions = ();
    my @privateHeaderFunctions = ();
    my @deprecatedHeaderFunctions = ();

    # - Add functions.
    if ($numFunctions > 0) {
        foreach my $function (@{$dataNode->functions}) {
            my $functionName = $function->signature->name;

            my $returnType = GetObjCType($function->signature->type);
            my $needsDeprecatedVersion = (@{$function->parameters} > 1 and $function->signature->extendedAttributes->{"OldStyleObjC"});
            my $numberOfParameters = @{$function->parameters};
            my %typesToForwardDeclare = ($function->signature->type => 1);

            my $parameterIndex = 0;
            my $functionSig = "- ($returnType)$functionName";
            my $methodName = $functionName;
            foreach my $param (@{$function->parameters}) {
                my $paramName = $param->name;
                my $paramType = GetObjCType($param->type);

                $typesToForwardDeclare{$param->type} = 1;

                if ($parameterIndex >= 1) {
                    my $paramPrefix = $param->extendedAttributes->{"ObjCPrefix"};
                    $paramPrefix = $paramName unless defined($paramPrefix);
                    $functionSig .= " $paramPrefix";
                    $methodName .= $paramPrefix;
                }

                $functionSig .= ":($paramType)$paramName";
                $methodName .= ":";

                $parameterIndex++;
            }

            my $publicInterfaceKey = $functionSig . ";";

            my $conflict = $conflictMethod{$methodName};
            if ($conflict) {
                warn "$className conflicts with $conflict method $methodName\n";
                $fatalError = 1;
            }

            if ($isProtocol && !$newPublicClass && !defined $publicInterfaces{$publicInterfaceKey}) {
                warn "Protocol method $publicInterfaceKey is not in PublicDOMInterfaces.h. Protocols require all methods to be public";
                $fatalError = 1;
            }

            my $availabilityMacro = "";
            if (defined $publicInterfaces{$publicInterfaceKey} and length $publicInterfaces{$publicInterfaceKey}) {
                $availabilityMacro = $publicInterfaces{$publicInterfaceKey};
            }

            $availabilityMacro = "WEBKIT_OBJC_METHOD_ANNOTATION($availabilityMacro)" if length $availabilityMacro and $buildingForTigerOrEarlier;

            my $functionDeclaration = $functionSig;
            $functionDeclaration .= " " . $availabilityMacro if length $availabilityMacro;
            $functionDeclaration .= ";\n";

            my $public = (defined $publicInterfaces{$publicInterfaceKey} or $newPublicClass);
            delete $publicInterfaces{$publicInterfaceKey};

            foreach my $type (keys %typesToForwardDeclare) {
                # add any forward declarations to the public header if a deprecated version will be generated
                AddForwardDeclarationsForType($type, 1) if $needsDeprecatedVersion;
                AddForwardDeclarationsForType($type, $public) unless $public and $needsDeprecatedVersion;
            }

            push(@headerFunctions, $functionDeclaration) if $public;
            push(@privateHeaderFunctions, $functionDeclaration) unless $public;

            # generate the old style method names with un-named parameters, these methods are deprecated
            if ($needsDeprecatedVersion) {
                my $deprecatedFunctionSig = $functionSig;
                $deprecatedFunctionSig =~ s/\s\w+:/ :/g; # remove parameter names

                $publicInterfaceKey = $deprecatedFunctionSig . ";";

                my $availabilityMacro = "AVAILABLE_WEBKIT_VERSION_1_3_AND_LATER_BUT_DEPRECATED_IN_WEBKIT_VERSION_3_0";
                if (defined $publicInterfaces{$publicInterfaceKey} and length $publicInterfaces{$publicInterfaceKey}) {
                    $availabilityMacro = $publicInterfaces{$publicInterfaceKey};
                }

                $availabilityMacro = "WEBKIT_OBJC_METHOD_ANNOTATION($availabilityMacro)" if $buildingForTigerOrEarlier;

                $functionDeclaration = "$deprecatedFunctionSig $availabilityMacro;\n";

                push(@deprecatedHeaderFunctions, $functionDeclaration);

                unless (defined $publicInterfaces{$publicInterfaceKey}) {
                    warn "Deprecated method $publicInterfaceKey is not in PublicDOMInterfaces.h. All deprecated methods need to be public, or should have the OldStyleObjC IDL attribute removed";
                    $fatalError = 1;
                }

                delete $publicInterfaces{$publicInterfaceKey};
            }
        }

        if (@headerFunctions > 0) {
            push(@headerContent, "\n") if $buildingForLeopardOrLater and @headerAttributes > 0;
            push(@headerContent, @headerFunctions);
        }
    }

    if (@deprecatedHeaderFunctions > 0 && $isProtocol) {
        push(@headerContent, @deprecatedHeaderFunctions);
    }

    # - End @interface or @protocol
    push(@headerContent, "\@end\n");

    if (@deprecatedHeaderFunctions > 0 && !$isProtocol) {
        # - Deprecated category @interface 
        push(@headerContent, "\n\@interface $className (" . $className . "Deprecated)\n");
        push(@headerContent, @deprecatedHeaderFunctions);
        push(@headerContent, "\@end\n");
    }

    push(@headerContent, "\n#endif\n") if length $interfaceAvailabilityVersion;

    my %alwaysGenerateForNoSVGBuild = map { $_ => 1 } qw(DOMHTMLEmbedElement DOMHTMLObjectElement);

    if (@privateHeaderAttributes > 0 or @privateHeaderFunctions > 0 or exists $alwaysGenerateForNoSVGBuild{$className}) {
        # - Private category @interface
        @privateHeaderContentHeader = split("\r", $headerLicenseTemplate);
        push(@privateHeaderContentHeader, "\n");

        my $classHeaderName = GetClassHeaderName($className);
        push(@privateHeaderContentHeader, "#import <WebCore/$classHeaderName.h>\n\n");
        push(@privateHeaderContentHeader, $interfaceAvailabilityVersionCheck) if length $interfaceAvailabilityVersion;

        @privateHeaderContent = ();
        push(@privateHeaderContent, "\@interface $className (" . $className . "Private)\n");
        push(@privateHeaderContent, @privateHeaderAttributes) if @privateHeaderAttributes > 0;
        push(@privateHeaderContent, "\n") if $buildingForLeopardOrLater and @privateHeaderAttributes > 0 and @privateHeaderFunctions > 0;
        push(@privateHeaderContent, @privateHeaderFunctions) if @privateHeaderFunctions > 0;
        push(@privateHeaderContent, "\@end\n");

        push(@privateHeaderContent, "\n#endif\n") if length $interfaceAvailabilityVersion;
    }

    unless ($isProtocol) {
        # Generate internal interfaces
        my $podType = $dataNode->extendedAttributes->{"PODType"};

        # Generate interface definitions. 
        @internalHeaderContent = split("\r", $implementationLicenseTemplate);

        push(@internalHeaderContent, "\n#import <WebCore/$className.h>\n\n");
        push(@internalHeaderContent, $interfaceAvailabilityVersionCheck) if length $interfaceAvailabilityVersion;

        if ($interfaceName eq "Node") {
            push(@internalHeaderContent, "\@protocol DOMEventTarget;\n\n");
        }

        my $startedNamespace = 0;

        my $implClassName = GetImplClassName($interfaceName);

        if ($codeGenerator->IsSVGAnimatedType($interfaceName)) {
            push(@internalHeaderContent, "#import <WebCore/SVGAnimatedTemplate.h>\n\n");
        } else {
            push(@internalHeaderContent, "namespace WebCore {\n");
            $startedNamespace = 1;
            if ($podType and $podType ne "float") {
                push(@internalHeaderContent, "    class $podType;\n");
            } elsif ($interfaceName eq "Node") {
                push(@internalHeaderContent, "    class EventTarget;\n    class Node;\n");
            } else {
                push(@internalHeaderContent, "    class $implClassName;\n");
            }
            push(@internalHeaderContent, "}\n\n");
        }

        if ($podType) {
            if ($podType eq "float") {
                push(@internalHeaderContent, "float core($className *);\n");
            } else {
                push(@internalHeaderContent, "WebCore::$podType core($className *);\n");
            }
        } else {
            push(@internalHeaderContent, "WebCore::$implClassName* core($className *);\n");
        }

        if ($podType) {
            if ($podType eq "float") {
                push(@internalHeaderContent, "$className *kit($podType);\n");
            } else {
                push(@internalHeaderContent, "$className *kit(WebCore::$podType);\n");
            }
        } else {
            push(@internalHeaderContent, "$className *kit(WebCore::$implClassName*);\n");
        }

        if ($dataNode->extendedAttributes->{Polymorphic}) {
            push(@internalHeaderContent, "Class kitClass(WebCore::$implClassName*);\n");
        }

        if ($interfaceName eq "Node") {
            push(@internalHeaderContent, "id <DOMEventTarget> kit(WebCore::EventTarget*);\n");
        }

        push(@internalHeaderContent, "\n#endif\n") if length $interfaceAvailabilityVersion;
    }
}

sub GenerateImplementation
{
    my $object = shift;
    my $dataNode = shift;

    my @ancestorInterfaceNames = ();

    if (@{$dataNode->parents} > 1) {
        $codeGenerator->AddMethodsConstantsAndAttributesFromParentClasses($dataNode, \@ancestorInterfaceNames);
    }

    my $interfaceName = $dataNode->name;
    my $className = GetClassName($interfaceName);
    my $implClassName = GetImplClassName($interfaceName);
    my $parentImplClassName = GetParentImplClassName($dataNode);
    my $implClassNameWithNamespace = "WebCore::" . $implClassName;
    my $baseClass = GetBaseClass($parentImplClassName);
    my $classHeaderName = GetClassHeaderName($className);
    my $conditional = $dataNode->extendedAttributes->{"Conditional"};

    my $numAttributes = @{$dataNode->attributes};
    my $numFunctions = @{$dataNode->functions};

    my $podType = $dataNode->extendedAttributes->{"PODType"};
    my $podTypeWithNamespace;

    if ($podType) {
        $podTypeWithNamespace = ($podType eq "float") ? "$podType" : "WebCore::$podType";
    }

    # - Add default header template.
    @implContentHeader = split("\r", $implementationLicenseTemplate);

    # - INCLUDES -
    push(@implContentHeader, "\n#import \"config.h\"\n");

    my $conditionalString;
    if ($conditional) {
        $conditionalString = "ENABLE(" . join(") && ENABLE(", split(/&/, $conditional)) . ")";
        push(@implContentHeader, "\n#if ${conditionalString}\n\n");
    }

    push(@implContentHeader, "#import \"DOMInternal.h\"\n\n");
    push(@implContentHeader, "#import \"$classHeaderName.h\"\n\n");

    $implIncludes{"ExceptionHandlers.h"} = 1;
    $implIncludes{"ThreadCheck.h"} = 1;
    $implIncludes{"WebScriptObjectPrivate.h"} = 1;
    $implIncludes{$classHeaderName . "Internal.h"} = 1;

    # FIXME: These includes are only needed when the class is a subclass of one of these polymorphic classes.
    $implIncludes{"DOMBlobInternal.h"} = 1;
    $implIncludes{"DOMCSSRuleInternal.h"} = 1;
    $implIncludes{"DOMCSSValueInternal.h"} = 1;
    $implIncludes{"DOMEventInternal.h"} = 1;
    $implIncludes{"DOMNodeInternal.h"} = 1;
    $implIncludes{"DOMStyleSheetInternal.h"} = 1;

    $implIncludes{"DOMSVGPathSegInternal.h"} = 1 if $interfaceName =~ /^SVGPathSeg.+/;

    if ($codeGenerator->IsSVGAnimatedType($interfaceName)) {
        $implIncludes{"SVGAnimatedTemplate.h"} = 1;
    } elsif ($interfaceName =~ /(\w+)(Abs|Rel)$/) {
        $implIncludes{"$1.h"} = 1;
    } else {
        if (!$podType) {
            $implIncludes{"$implClassName.h"} = 1;
        } else {
            $implIncludes{"$podType.h"} = 1 unless $podType eq "float";
        }
    } 

    @implContent = ();

    push(@implContent, "#import <wtf/GetPtr.h>\n\n");

    # add implementation accessor
    if ($podType) {
        push(@implContent, "#define IMPL reinterpret_cast<$podTypeWithNamespace*>(_internal)\n\n");
    } elsif ($parentImplClassName eq "Object") {
        push(@implContent, "#define IMPL reinterpret_cast<$implClassNameWithNamespace*>(_internal)\n\n");
    } else {
        my $baseClassWithNamespace = "WebCore::$baseClass";
        push(@implContent, "#define IMPL static_cast<$implClassNameWithNamespace*>(reinterpret_cast<$baseClassWithNamespace*>(_internal))\n\n");
    }

    # START implementation
    push(@implContent, "\@implementation $className\n\n");

    # Only generate 'dealloc' and 'finalize' methods for direct subclasses of DOMObject.
    if ($parentImplClassName eq "Object") {
        $implIncludes{"WebCoreObjCExtras.h"} = 1;
        push(@implContent, "- (void)dealloc\n");
        push(@implContent, "{\n");
        push(@implContent, "    if (WebCoreObjCScheduleDeallocateOnMainThread([$className class], self))\n");
        push(@implContent, "        return;\n");
        push(@implContent, "\n");
        if ($interfaceName eq "NodeIterator") {
            push(@implContent, "    if (_internal) {\n");
            push(@implContent, "        [self detach];\n");
            push(@implContent, "        IMPL->deref();\n");
            push(@implContent, "    };\n");
        } elsif ($podType) {
            push(@implContent, "    delete IMPL;\n");
        } else {
            push(@implContent, "    if (_internal)\n");
            push(@implContent, "        IMPL->deref();\n");
        }
        push(@implContent, "    [super dealloc];\n");
        push(@implContent, "}\n\n");

        push(@implContent, "- (void)finalize\n");
        push(@implContent, "{\n");
        if ($interfaceName eq "NodeIterator") {
            push(@implContent, "    if (_internal) {\n");
            push(@implContent, "        [self detach];\n");
            push(@implContent, "        IMPL->deref();\n");
            push(@implContent, "    };\n");
        } elsif ($podType) {
            push(@implContent, "    delete IMPL;\n");
        } else {
            push(@implContent, "    if (_internal)\n");
            push(@implContent, "        IMPL->deref();\n");
        }
        push(@implContent, "    [super finalize];\n");
        push(@implContent, "}\n\n");
        
    }

    %attributeNames = ();

    # - Attributes
    if ($numAttributes > 0) {
        foreach my $attribute (@{$dataNode->attributes}) {
            AddIncludesForType($attribute->signature->type);

            my $idlType = $codeGenerator->StripModule($attribute->signature->type);

            my $attributeName = $attribute->signature->name;
            my $attributeType = GetObjCType($attribute->signature->type);
            my $attributeIsReadonly = ($attribute->type =~ /^readonly/);
            my $attributeClassName = GetClassName($attribute->signature->type);

            my $attributeInterfaceName = $attributeName;
            if ($attributeName eq "id" or $attributeName eq "hash") {
                # Special case attributes id and hash to be idName and hashName to avoid ObjC naming conflict.
                $attributeInterfaceName .= "Name";
            } elsif ($attributeName eq "frame") {
                # Special case attribute frame to be frameBorders.
                $attributeInterfaceName .= "Borders";
            } elsif ($attributeName eq "ownerDocument") {
                # FIXME: for now special case attribute ownerDocument to call document, this is incorrect
                # legacy behavior. (see http://bugs.webkit.org/show_bug.cgi?id=10889)
                $attributeName = "document";
            } elsif ($codeGenerator->IsSVGAnimatedType($idlType)) {
                # Special case for animated types.
                $attributeName .= "Animated";
            }

            $attributeNames{$attributeInterfaceName} = 1;

            # - GETTER
            my $getterSig = "- ($attributeType)$attributeInterfaceName\n";

            # Some SVGFE*Element.idl use 'operator' as attribute name, rewrite as '_operator' to avoid clashes with C/C++
            $attributeName =~ s/operatorAnimated/_operatorAnimated/ if ($attributeName =~ /operatorAnimated/);
            $getterSig =~ s/operator/_operator/ if ($getterSig =~ /operator/);

            my $hasGetterException = @{$attribute->getterExceptions};
            my $getterContentHead;
            my $reflect = $attribute->signature->extendedAttributes->{"Reflect"};
            my $reflectURL = $attribute->signature->extendedAttributes->{"ReflectURL"};
            if ($reflect || $reflectURL) {
                my $contentAttributeName = (($reflect || $reflectURL) eq "1") ? $attributeName : ($reflect || $reflectURL);
                my $namespace = $codeGenerator->NamespaceForAttributeName($interfaceName, $contentAttributeName);
                $implIncludes{"${namespace}.h"} = 1;
                my $getAttributeFunctionName = $reflectURL ? "getURLAttribute" : "getAttribute";
                $getterContentHead = "IMPL->${getAttributeFunctionName}(WebCore::${namespace}::${contentAttributeName}Attr";
            } else {
                $getterContentHead = "IMPL->" . $codeGenerator->WK_lcfirst($attributeName) . "(";
            }
            my $getterContentTail = ")";

            # Special case for DOMSVGNumber
            if ($podType and $podType eq "float") {
                $getterContentHead = "*IMPL";
                $getterContentTail = "";
            }

            # TODO: Handle special case for DOMSVGLength
            if ($podType and $podType eq "SVGLength" and $attributeName eq "value") {
                $getterContentHead = "IMPL->value(0 /* FIXME */";
            }

            my $attributeTypeSansPtr = $attributeType;
            $attributeTypeSansPtr =~ s/ \*$//; # Remove trailing " *" from pointer types.

            # special case for EventTarget protocol
            $attributeTypeSansPtr = "DOMNode" if $idlType eq "EventTarget";

            # Special cases
            my @customGetterContent = (); 
            if ($attributeTypeSansPtr eq "DOMImplementation") {
                # FIXME: We have to special case DOMImplementation until DOMImplementationFront is removed
                $getterContentHead = "kit(implementationFront(IMPL";
                $getterContentTail .= ")";
            } elsif ($attributeName =~ /(\w+)DisplayString$/) {
                my $attributeToDisplay = $1;
                $getterContentHead = "WebCore::displayString(IMPL->$attributeToDisplay(), core(self)";
                $implIncludes{"HitTestResult.h"} = 1;
            } elsif ($attributeName =~ /^absolute(\w+)URL$/) {
                my $typeOfURL = $1;
                $getterContentHead = "[self _getURLAttribute:";
                if ($typeOfURL eq "Link") {
                    $getterContentTail = "\@\"href\"]";
                } elsif ($typeOfURL eq "Image") {
                    if ($interfaceName eq "HTMLObjectElement") {
                        $getterContentTail = "\@\"data\"]";
                    } else {
                        $getterContentTail = "\@\"src\"]";
                    }
                    unless ($interfaceName eq "HTMLImageElement") {
                        push(@customGetterContent, "    if (!IMPL->renderer() || !IMPL->renderer()->isImage())\n");
                        push(@customGetterContent, "        return nil;\n");
                        $implIncludes{"RenderObject.h"} = 1;
                    }
                }
                $implIncludes{"DOMPrivate.h"} = 1;
            } elsif ($attribute->signature->extendedAttributes->{"ConvertToString"}) {
                $getterContentHead = "WebCore::String::number(" . $getterContentHead;
                $getterContentTail .= ")";
            } elsif ($attribute->signature->extendedAttributes->{"ConvertFromString"}) {
                $getterContentTail .= ".toInt()";
            } elsif ($codeGenerator->IsPodType($idlType) or $idlType eq "Date") {
                $getterContentHead = "kit($getterContentHead";
                $getterContentTail .= ")";
            } elsif (IsProtocolType($idlType) and $idlType ne "EventTarget") {
                $getterContentHead = "kit($getterContentHead";
                $getterContentTail .= ")";
            } elsif ($idlType eq "Color") {
                $getterContentHead = "WebCore::nsColor($getterContentHead";
                $getterContentTail .= ")";
            } elsif ($attribute->signature->type eq "SerializedScriptValue") {
                $getterContentHead = "$getterContentHead";
                $getterContentTail .= "->toString()";                
            } elsif (ConversionNeeded($attribute->signature->type)) {
                $getterContentHead = "kit(WTF::getPtr($getterContentHead";
                $getterContentTail .= "))";
            }

            my $getterContent;
            if ($hasGetterException) {
                $getterContent = $getterContentHead . "ec" . $getterContentTail;
            } else {
                $getterContent = $getterContentHead . $getterContentTail;
            }

            push(@implContent, $getterSig);
            push(@implContent, "{\n");
            push(@implContent, @customGetterContent);
            if ($hasGetterException) {
                # Differentiated between when the return type is a pointer and
                # not for white space issue (ie. Foo *result vs. int result).
                if ($attributeType =~ /\*$/) {
                    $getterContent = $attributeType . "result = " . $getterContent;
                } else {
                    $getterContent = $attributeType . " result = " . $getterContent;
                }

                push(@implContent, "    $exceptionInit\n");
                push(@implContent, "    $getterContent;\n");
                push(@implContent, "    $exceptionRaiseOnError\n");
                push(@implContent, "    return result;\n");
            } else {
                push(@implContent, "    return $getterContent;\n");
            }
            push(@implContent, "}\n\n");

            # - SETTER
            if (!$attributeIsReadonly) {
                # Exception handling
                my $hasSetterException = @{$attribute->setterExceptions};

                my $coreSetterName = "set" . $codeGenerator->WK_ucfirst($attributeName);
                my $setterName = "set" . ucfirst($attributeInterfaceName);
                my $argName = "new" . ucfirst($attributeInterfaceName);
                my $arg = GetObjCTypeGetter($argName, $idlType);

                # The definition of ConvertFromString and ConvertToString is flipped for the setter
                if ($attribute->signature->extendedAttributes->{"ConvertFromString"}) {
                    $arg = "WebCore::String::number($arg)";
                } elsif ($attribute->signature->extendedAttributes->{"ConvertToString"}) {
                    $arg = "WebCore::String($arg).toInt()";
                }

                my $setterSig = "- (void)$setterName:($attributeType)$argName\n";

                push(@implContent, $setterSig);
                push(@implContent, "{\n");

                unless ($codeGenerator->IsPrimitiveType($idlType) or $codeGenerator->IsStringType($idlType)) {
                    push(@implContent, "    ASSERT($argName);\n\n");
                }

                if ($idlType eq "Date") {
                    $arg = "core(" . $arg . ")";
                }

                if ($podType) {
                    # Special case for DOMSVGNumber
                    if ($podType eq "float") {
                        push(@implContent, "    *IMPL = $arg;\n");
                    } else {
                        push(@implContent, "    IMPL->$coreSetterName($arg);\n");
                    }
                } else {
                    my $reflect = $attribute->signature->extendedAttributes->{"Reflect"};
                    my $reflectURL = $attribute->signature->extendedAttributes->{"ReflectURL"};
                    push(@implContent, "    $exceptionInit\n") if $hasSetterException;
                    my $ec = $hasSetterException ? ", ec" : "";
                    if ($reflect || $reflectURL) {
                        my $contentAttributeName = (($reflect || $reflectURL) eq "1") ? $attributeName : ($reflect || $reflectURL);
                        my $namespace = $codeGenerator->NamespaceForAttributeName($interfaceName, $contentAttributeName);
                        $implIncludes{"${namespace}.h"} = 1;
                        push(@implContent, "    IMPL->setAttribute(WebCore::${namespace}::${contentAttributeName}Attr, $arg$ec);\n");
                    } else {
                        push(@implContent, "    IMPL->$coreSetterName($arg$ec);\n");
                    }
                    push(@implContent, "    $exceptionRaiseOnError\n") if $hasSetterException;
                }

                push(@implContent, "}\n\n");
            }
        }
    }

    # - Functions
    if ($numFunctions > 0) {
        foreach my $function (@{$dataNode->functions}) {
            AddIncludesForType($function->signature->type);

            my $functionName = $function->signature->name;
            my $returnType = GetObjCType($function->signature->type);
            my $hasParameters = @{$function->parameters};
            my $raisesExceptions = @{$function->raisesExceptions};

            my @parameterNames = ();
            my @needsAssert = ();
            my %needsCustom = ();

            my $parameterIndex = 0;
            my $functionSig = "- ($returnType)$functionName";
            foreach my $param (@{$function->parameters}) {
                my $paramName = $param->name;
                my $paramType = GetObjCType($param->type);

                # make a new parameter name if the original conflicts with a property name
                $paramName = "in" . ucfirst($paramName) if $attributeNames{$paramName};

                AddIncludesForType($param->type);

                my $idlType = $codeGenerator->StripModule($param->type);
                my $implGetter = GetObjCTypeGetter($paramName, $idlType);

                push(@parameterNames, $implGetter);
                $needsCustom{"XPathNSResolver"} = $paramName if $idlType eq "XPathNSResolver";
                $needsCustom{"NodeFilter"} = $paramName if $idlType eq "NodeFilter";
                $needsCustom{"EventListener"} = $paramName if $idlType eq "EventListener";
                $needsCustom{"EventTarget"} = $paramName if $idlType eq "EventTarget";
                $needsCustom{"NodeToReturn"} = $paramName if $param->extendedAttributes->{"Return"};

                unless ($codeGenerator->IsPrimitiveType($idlType) or $codeGenerator->IsStringType($idlType)) {
                    push(@needsAssert, "    ASSERT($paramName);\n");
                }

                if ($parameterIndex >= 1) {
                    my $paramPrefix = $param->extendedAttributes->{"ObjCPrefix"};
                    $paramPrefix = $param->name unless defined($paramPrefix);
                    $functionSig .= " $paramPrefix";
                }

                $functionSig .= ":($paramType)$paramName";

                $parameterIndex++;
            }

            my @functionContent = ();
            my $caller = "IMPL";

            # special case the XPathNSResolver
            if (defined $needsCustom{"XPathNSResolver"}) {
                my $paramName = $needsCustom{"XPathNSResolver"};
                push(@functionContent, "    WebCore::XPathNSResolver* nativeResolver = 0;\n");
                push(@functionContent, "    RefPtr<WebCore::XPathNSResolver> customResolver;\n");
                push(@functionContent, "    if ($paramName) {\n");
                push(@functionContent, "        if ([$paramName isMemberOfClass:[DOMNativeXPathNSResolver class]])\n");
                push(@functionContent, "            nativeResolver = core(static_cast<DOMNativeXPathNSResolver *>($paramName));\n");
                push(@functionContent, "        else {\n");
                push(@functionContent, "            customResolver = WebCore::DOMCustomXPathNSResolver::create($paramName);\n");
                push(@functionContent, "            nativeResolver = WTF::getPtr(customResolver);\n");
                push(@functionContent, "        }\n");
                push(@functionContent, "    }\n");
            }

            # special case the EventTarget
            if (defined $needsCustom{"EventTarget"}) {
                my $paramName = $needsCustom{"EventTarget"};
                push(@functionContent, "    DOMNode* ${paramName}ObjC = $paramName;\n");
                push(@functionContent, "    WebCore::Node* ${paramName}Node = core(${paramName}ObjC);\n");
                $implIncludes{"DOMNode.h"} = 1;
                $implIncludes{"Node.h"} = 1;
            }

            if ($function->signature->extendedAttributes->{"UsesView"}) {
                push(@functionContent, "    WebCore::DOMWindow* dv = $caller->defaultView();\n");
                push(@functionContent, "    if (!dv)\n");
                push(@functionContent, "        return nil;\n");
                $implIncludes{"DOMWindow.h"} = 1;
                $caller = "dv";
            }

            # special case the EventListener
            if (defined $needsCustom{"EventListener"}) {
                my $paramName = $needsCustom{"EventListener"};
                push(@functionContent, "    RefPtr<WebCore::EventListener> nativeEventListener = WebCore::ObjCEventListener::wrap($paramName);\n");
            }

            # special case the NodeFilter
            if (defined $needsCustom{"NodeFilter"}) {
                my $paramName = $needsCustom{"NodeFilter"};
                push(@functionContent, "    RefPtr<WebCore::NodeFilter> nativeNodeFilter;\n");
                push(@functionContent, "    if ($paramName)\n");
                push(@functionContent, "        nativeNodeFilter = WebCore::NodeFilter::create(WebCore::ObjCNodeFilterCondition::create($paramName));\n");
            }

            # FIXME! We need [Custom] support for ObjC, to move these hacks into DOMSVGLength/MatrixCustom.mm
            my $svgMatrixRotateFromVector = ($podType and $podType eq "AffineTransform" and $functionName eq "rotateFromVector");
            my $svgMatrixInverse = ($podType and $podType eq "AffineTransform" and $functionName eq "inverse");
            my $svgLengthConvertToSpecifiedUnits = ($podType and $podType eq "SVGLength" and $functionName eq "convertToSpecifiedUnits");

            push(@parameterNames, "ec") if $raisesExceptions and !($svgMatrixRotateFromVector || $svgMatrixInverse);
            my $content = $caller . "->" . $codeGenerator->WK_lcfirst($functionName) . "(" . join(", ", @parameterNames) . ")"; 

            if ($svgMatrixRotateFromVector) {
                # Special case with rotateFromVector & SVGMatrix        
                push(@functionContent, "    $exceptionInit\n");
                push(@functionContent, "    if (x == 0.0 || y == 0.0)\n");
                push(@functionContent, "        ec = WebCore::SVGException::SVG_INVALID_VALUE_ERR;\n");
                push(@functionContent, "    $exceptionRaiseOnError\n");
                push(@functionContent, "    return kit($content);\n");
            } elsif ($svgMatrixInverse) {
                # Special case with inverse & SVGMatrix
                push(@functionContent, "    $exceptionInit\n");
                push(@functionContent, "    if (!$caller->isInvertible())\n");
                push(@functionContent, "        ec = WebCore::SVGException::SVG_MATRIX_NOT_INVERTABLE;\n");
                push(@functionContent, "    $exceptionRaiseOnError\n");
                push(@functionContent, "    return kit($content);\n");
            } elsif ($svgLengthConvertToSpecifiedUnits) {
                push(@functionContent, "    IMPL->convertToSpecifiedUnits(inUnitType, 0 /* FIXME */);\n");
            } elsif ($returnType eq "void") {
                # Special case 'void' return type.
                if ($raisesExceptions) {
                    push(@functionContent, "    $exceptionInit\n");
                    push(@functionContent, "    $content;\n");
                    push(@functionContent, "    $exceptionRaiseOnError\n");
                } else {
                    push(@functionContent, "    $content;\n");
                }
            } elsif (defined $needsCustom{"NodeToReturn"}) {
                # Special case the insertBefore, replaceChild, removeChild 
                # and appendChild functions from DOMNode 
                my $toReturn = $needsCustom{"NodeToReturn"};
                if ($raisesExceptions) {
                    push(@functionContent, "    $exceptionInit\n");
                    push(@functionContent, "    if ($content)\n");
                    push(@functionContent, "        return $toReturn;\n");
                    push(@functionContent, "    $exceptionRaiseOnError\n");
                    push(@functionContent, "    return nil;\n");
                } else {
                    push(@functionContent, "    if ($content)\n");
                    push(@functionContent, "        return $toReturn;\n");
                    push(@functionContent, "    return nil;\n");
                }
            } elsif ($returnType eq "SerializedScriptValue") {
                $content = "foo";
            } else {
                if (ConversionNeeded($function->signature->type)) {
                    if ($codeGenerator->IsPodType($function->signature->type)) {
                        $content = "kit($content)";
                    } else {
                        $content = "kit(WTF::getPtr($content))";
                    }
                }

                if ($raisesExceptions) {
                    # Differentiated between when the return type is a pointer and
                    # not for white space issue (ie. Foo *result vs. int result).
                    if ($returnType =~ /\*$/) {
                        $content = $returnType . "result = " . $content;
                    } else {
                        $content = $returnType . " result = " . $content;
                    }

                    push(@functionContent, "    $exceptionInit\n");
                    push(@functionContent, "    $content;\n");
                    push(@functionContent, "    $exceptionRaiseOnError\n");
                    push(@functionContent, "    return result;\n");
                } else {
                    push(@functionContent, "    return $content;\n");
                }
            }

            push(@implContent, "$functionSig\n");
            push(@implContent, "{\n");
            push(@implContent, @functionContent);
            push(@implContent, "}\n\n");

            # generate the old style method names with un-named parameters, these methods are deprecated
            if (@{$function->parameters} > 1 and $function->signature->extendedAttributes->{"OldStyleObjC"}) {
                my $deprecatedFunctionSig = $functionSig;
                $deprecatedFunctionSig =~ s/\s\w+:/ :/g; # remove parameter names

                push(@implContent, "$deprecatedFunctionSig\n");
                push(@implContent, "{\n");
                push(@implContent, @functionContent);
                push(@implContent, "}\n\n");
            }

            # Clear the hash
            %needsCustom = ();
        }
    }

    # END implementation
    push(@implContent, "\@end\n");

    # Generate internal interfaces
    if ($podType) {
        my $prefixedPodType = $podType eq "float" ? $podType : "WebCore::$podType";
        push(@implContent, "\n$prefixedPodType core($className *wrapper)\n");
        push(@implContent, "{\n");
        push(@implContent, "    return wrapper ? *reinterpret_cast<$prefixedPodType*>(wrapper->_internal) : $prefixedPodType();\n");
        push(@implContent, "}\n\n");
    } else {
        push(@implContent, "\nWebCore::$implClassName* core($className *wrapper)\n");
        push(@implContent, "{\n");
        push(@implContent, "    return wrapper ? reinterpret_cast<WebCore::$implClassName*>(wrapper->_internal) : 0;\n");
        push(@implContent, "}\n\n");
    }

    if ($podType) {
        # FIXME: Implement caching.
        my $prefixedPodType = $podType eq "float" ? $podType : "WebCore::$podType";
        push(@implContent, "$className *kit($prefixedPodType value)\n");
        push(@implContent, "{\n");
        push(@implContent, "    $assertMainThread;\n");
        push(@implContent, "    $className *wrapper = [[$className alloc] _init];\n");
        push(@implContent, "    wrapper->_internal = reinterpret_cast<DOMObjectInternal*>(new $prefixedPodType(value));\n");
        push(@implContent, "    return [wrapper autorelease];\n");
        push(@implContent, "}\n");
    } elsif ($parentImplClassName eq "Object") {        
        push(@implContent, "$className *kit(WebCore::$implClassName* value)\n");
        push(@implContent, "{\n");
        push(@implContent, "    $assertMainThread;\n");
        push(@implContent, "    if (!value)\n");
        push(@implContent, "        return nil;\n");
        push(@implContent, "    if ($className *wrapper = getDOMWrapper(value))\n");
        push(@implContent, "        return [[wrapper retain] autorelease];\n");
        if ($dataNode->extendedAttributes->{Polymorphic}) {
            push(@implContent, "    $className *wrapper = [[kitClass(value) alloc] _init];\n");
            push(@implContent, "    if (!wrapper)\n");
            push(@implContent, "        return nil;\n");
        } else {
            push(@implContent, "    $className *wrapper = [[$className alloc] _init];\n");
        }
        push(@implContent, "    wrapper->_internal = reinterpret_cast<DOMObjectInternal*>(value);\n");
        push(@implContent, "    value->ref();\n");
        push(@implContent, "    addDOMWrapper(wrapper, value);\n");
        push(@implContent, "    return [wrapper autorelease];\n");
        push(@implContent, "}\n");
    } else {
        push(@implContent, "$className *kit(WebCore::$implClassName* value)\n");
        push(@implContent, "{\n");
        push(@implContent, "    $assertMainThread;\n");
        push(@implContent, "    return static_cast<$className*>(kit(static_cast<WebCore::$baseClass*>(value)));\n");
        push(@implContent, "}\n");
    }

    # - End the ifdef conditional if necessary
    push(@implContent, "\n#endif // ${conditionalString}\n") if $conditional;

    # - Generate dependencies.
    if ($writeDependencies && @ancestorInterfaceNames) {
        push(@depsContent, "$className.h : ", join(" ", map { "$_.idl" } @ancestorInterfaceNames), "\n");
        push(@depsContent, map { "$_.idl :\n" } @ancestorInterfaceNames); 
    }
}

# Internal helper
sub WriteData
{
    my $object = shift;
    my $name = shift;

    # Open files for writing...
    my $headerFileName = "$outputDir/" . $name . ".h";
    my $privateHeaderFileName = "$outputDir/" . $name . "Private.h";
    my $implFileName = "$outputDir/" . $name . ".mm";
    my $internalHeaderFileName = "$outputDir/" . $name . "Internal.h";
    my $depsFileName = "$outputDir/" . $name . ".dep";

    # Remove old files.
    unlink($headerFileName);
    unlink($privateHeaderFileName);
    unlink($implFileName);
    unlink($internalHeaderFileName);
    unlink($depsFileName);

    # Write public header.
    open(HEADER, ">$headerFileName") or die "Couldn't open file $headerFileName";
    
    print HEADER @headerContentHeader;
    print HEADER map { "\@class $_;\n" } sort keys(%headerForwardDeclarations);
    print HEADER map { "\@protocol $_;\n" } sort keys(%headerForwardDeclarationsForProtocols);

    my $hasForwardDeclarations = keys(%headerForwardDeclarations) + keys(%headerForwardDeclarationsForProtocols);
    print HEADER "\n" if $hasForwardDeclarations;
    print HEADER @headerContent;

    close(HEADER);

    @headerContentHeader = ();
    @headerContent = ();
    %headerForwardDeclarations = ();
    %headerForwardDeclarationsForProtocols = ();

    if (@privateHeaderContent > 0) {
        open(PRIVATE_HEADER, ">$privateHeaderFileName") or die "Couldn't open file $privateHeaderFileName";

        print PRIVATE_HEADER @privateHeaderContentHeader;
        print PRIVATE_HEADER map { "\@class $_;\n" } sort keys(%privateHeaderForwardDeclarations);
        print PRIVATE_HEADER map { "\@protocol $_;\n" } sort keys(%privateHeaderForwardDeclarationsForProtocols);

        $hasForwardDeclarations = keys(%privateHeaderForwardDeclarations) + keys(%privateHeaderForwardDeclarationsForProtocols);
        print PRIVATE_HEADER "\n" if $hasForwardDeclarations;
        print PRIVATE_HEADER @privateHeaderContent;

        close(PRIVATE_HEADER);

        @privateHeaderContentHeader = ();
        @privateHeaderContent = ();
        %privateHeaderForwardDeclarations = ();
        %privateHeaderForwardDeclarationsForProtocols = ();
    }

    # Write implementation file.
    unless ($noImpl) {
        open(IMPL, ">$implFileName") or die "Couldn't open file $implFileName";

        print IMPL @implContentHeader;
        print IMPL map { "#import \"$_\"\n" } sort keys(%implIncludes);
        print IMPL @implContent;

        close(IMPL);

        @implContentHeader = ();
        @implContent = ();
        %implIncludes = ();
    }
    
    if (@internalHeaderContent > 0) {
       open(INTERNAL_HEADER, ">$internalHeaderFileName") or die "Couldn't open file $internalHeaderFileName";

       print INTERNAL_HEADER @internalHeaderContent;

       close(INTERNAL_HEADER);

       @internalHeaderContent = ();
    }

    # Write dependency file.
    if (@depsContent) {
        open(DEPS, ">$depsFileName") or die "Couldn't open file $depsFileName";
        print DEPS @depsContent;
        close(DEPS);
        @depsContent = ();
    }
}

1;
