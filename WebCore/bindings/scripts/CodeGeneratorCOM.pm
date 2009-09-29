#
# Copyright (C) 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
# Copyright (C) 2006 Anders Carlsson <andersca@mac.com>
# Copyright (C) 2006, 2007 Samuel Weinig <sam@webkit.org>
# Copyright (C) 2006 Alexey Proskuryakov <ap@webkit.org>
# Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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
# aint with this library; see the file COPYING.LIB.  If not, write to
# the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.

package CodeGeneratorCOM;

use File::stat;

# Global Variables
my $module = "";
my $outputDir = "";

my @IDLHeader = ();
my @IDLContent = ();
my %IDLIncludes = ();
my %IDLForwardDeclarations = ();
my %IDLDontForwardDeclare = ();
my %IDLImports = ();
my %IDLDontImport = ();

my @CPPInterfaceHeader = ();

my @CPPHeaderHeader = ();
my @CPPHeaderContent = ();
my %CPPHeaderIncludes = ();
my %CPPHeaderIncludesAngle = ();
my %CPPHeaderForwardDeclarations = ();
my %CPPHeaderDontForwardDeclarations = ();

my @CPPImplementationHeader = ();
my @CPPImplementationContent = ();
my %CPPImplementationIncludes = ();
my %CPPImplementationWebCoreIncludes = ();
my %CPPImplementationIncludesAngle = ();
my %CPPImplementationDontIncludes = ();

my @additionalInterfaceDefinitions = ();

my $DASHES = "----------------------------------------";
my $TEMP_PREFIX = "GEN_";

# Hashes

my %includeCorrector = map {($_, 1)} qw{UIEvent KeyboardEvent MouseEvent
                                        MutationEvent OverflowEvent WheelEvent};

my %conflictMethod = (
    # FIXME: Add C language keywords?
);

# Default License Templates
my @licenseTemplate = split(/\r/, << "EOF");
/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
EOF

# Default constructor
sub new
{
    my $object = shift;
    my $reference = { };

    $codeGenerator = shift;
    $outputDir = shift;

    bless($reference, $object);
    return $reference;
}

sub finish
{
    my $object = shift;
}

# Params: 'domClass' struct
sub GenerateInterface
{
    my $object = shift;
    my $dataNode = shift;
    my $defines = shift;

    my $name = $dataNode->name;

    my $pureInterface = $dataNode->extendedAttributes->{"PureInterface"};

    # Start actual generation..
    $object->GenerateIDL($dataNode, $pureInterface);
    if ($pureInterface) {
        $object->GenerateInterfaceHeader($dataNode);
    } else {
        $object->GenerateCPPHeader($dataNode);
        $object->GenerateCPPImplementation($dataNode);
    }

    # Write changes.
    $object->WriteData($name, $pureInterface);
}

# Params: 'idlDocument' struct
sub GenerateModule
{
    my $object = shift;
    my $dataNode = shift;

    $module = $dataNode->module;
}

sub GetInterfaceName
{
    my $name = $codeGenerator->StripModule(shift);

    die "GetInterfaceName should only be used on interfaces." if ($codeGenerator->IsStringType($name) or $codeGenerator->IsPrimitiveType($name));

    # special cases
    return "I" . $TEMP_PREFIX . "DOMAbstractView" if $name eq "DOMWindow";
    return "I" . $TEMP_PREFIX . $name if $name eq "DOMImplementation" or $name eq "DOMTimeStamp";

    # Default, assume COM type has the same type name as
    # idl type prefixed with "IDOM".
    return "I" . $TEMP_PREFIX . "DOM" . $name;
}

sub GetClassName
{
    my $name = $codeGenerator->StripModule(shift);

    # special cases
    return "BSTR" if $codeGenerator->IsStringType($name);
    return "BOOL" if $name eq "boolean";
    return "unsigned" if $name eq "unsigned long";
    return "int" if $name eq "long";
    return $name if $codeGenerator->IsPrimitiveType($name);
    return $TEMP_PREFIX . "DOMAbstractView" if $name eq "DOMWindow";
    return $TEMP_PREFIX . $name if $name eq "DOMImplementation" or $name eq "DOMTimeStamp";

    # Default, assume COM type has the same type name as
    # idl type prefixed with "DOM".
    return $TEMP_PREFIX . "DOM" . $name;
}

sub GetCOMType
{
    my ($type) = @_;

    die "Don't use GetCOMType for string types, use one of In/Out variants instead." if $codeGenerator->IsStringType($type);

    return "BOOL" if $type eq "boolean";
    return "UINT" if $type eq "unsigned long";
    return "INT" if $type eq "long";
    return $type if $codeGenerator->IsPrimitiveType($type) or $type eq "DOMTimeStamp";
    # return "unsigned short" if $type eq "CompareHow" or $type eq "SVGPaintType";

    return GetInterfaceName($type) . "*";
}

sub GetCOMTypeIn
{
    my ($type) = @_;
    return "LPCTSTR" if $codeGenerator->IsStringType($type);
    return GetCOMType($type);
}

sub GetCOMTypeOut
{
    my ($type) = @_;
    return "BSTR" if $codeGenerator->IsStringType($type);
    return GetCOMType($type);
}

sub IDLTypeToImplementationType
{
    my $type = $codeGenerator->StripModule(shift);

    return "bool" if $type eq "boolean";
    return "unsigned" if $type eq "unsigned long";
    return "int" if $type eq "long";
    return $type if $codeGenerator->IsPrimitiveType($type);

    return "WebCore::String" if $codeGenerator->IsStringType($type);
    return "WebCore::${type}";
}

sub StripNamespace
{
    my ($type) = @_;

    $type =~ s/^WebCore:://;

    return $type;
}

sub GetParentInterface
{
    my ($dataNode) = @_;
    return "I" . $TEMP_PREFIX . "DOMObject" if (@{$dataNode->parents} == 0);
    return GetInterfaceName($codeGenerator->StripModule($dataNode->parents(0)));
}

sub GetParentClass
{
    my ($dataNode) = @_;
    return $TEMP_PREFIX . "DOMObject" if (@{$dataNode->parents} == 0);
    return GetClassName($codeGenerator->StripModule($dataNode->parents(0)));
}

sub AddForwardDeclarationsForTypeInIDL
{
    my $type = $codeGenerator->StripModule(shift);

    return if $codeGenerator->IsNonPointerType($type) or $codeGenerator->IsStringType($type);

    my $interface = GetInterfaceName($type);
    $IDLForwardDeclarations{$interface} = 1;
    $IDLImports{$interface} = 1;
}

sub AddIncludesForTypeInCPPHeader
{
    my $type = $codeGenerator->StripModule(shift);
    my $useAngleBrackets = shift;

    return if $codeGenerator->IsNonPointerType($type);

    # Add special Cases HERE

    if ($type =~ m/^I/) {
        $type = "WebKit";
    }

    if ($useAngleBrackets) {
        $CPPHeaderIncludesAngle{"$type.h"} = 1;
        return;
    }

    if ($type eq "GEN_DOMImplementation") {
        $CPPHeaderIncludes{"GEN_DOMDOMImplementation.h"} = 1;
        return;
    }

    if ($type eq "IGEN_DOMImplementation") {
        $CPPHeaderIncludes{"IGEN_DOMDOMImplementation.h"} = 1;
        return;
    }

    $CPPHeaderIncludes{"$type.h"} = 1;
}

sub AddForwardDeclarationsForTypeInCPPHeader
{
    my $type = $codeGenerator->StripModule(shift);

    return if $codeGenerator->IsNonPointerType($type) or $codeGenerator->IsStringType($type);

    my $interface = GetInterfaceName($type);
    $CPPHeaderForwardDeclarations{$interface} = 1;
}

sub AddIncludesForTypeInCPPImplementation
{
    my $type = $codeGenerator->StripModule(shift);

    die "Include type not supported!" if $includeCorrector{$type};

    return if $codeGenerator->IsNonPointerType($type);

    if ($codeGenerator->IsStringType($type)) {
        $CPPImplementationWebCoreIncludes{"AtomicString.h"} = 1;
        $CPPImplementationWebCoreIncludes{"BString.h"} = 1;
        $CPPImplementationWebCoreIncludes{"KURL.h"} = 1;
        return;
    }

    # Special casing
    $CPPImplementationWebCoreIncludes{"NameNodeList.h"} = 1 if $type eq "NodeList";
    $CPPImplementationWebCoreIncludes{"CSSMutableStyleDeclaration.h"} = 1 if $type eq "CSSStyleDeclaration";

    # Add implementation type
    $CPPImplementationWebCoreIncludes{StripNamespace(IDLTypeToImplementationType($type)) . ".h"} = 1;

    my $COMClassName = GetClassName($type);
    $CPPImplementationIncludes{"${COMClassName}.h"} = 1;
}

sub GetAdditionalInterfaces
{
    # This function does nothing, but it stays here for future multiple inheritance support.
    my $type = $codeGenerator->StripModule(shift);
    return ();
}

sub GenerateIDL
{
    my ($object, $dataNode, $pureInterface) = @_;

    my $inInterfaceName = $dataNode->name;
    my $outInterfaceName = GetInterfaceName($inInterfaceName);
    my $uuid = $dataNode->extendedAttributes->{"InterfaceUUID"} || die "All classes require an InterfaceUUID extended attribute.";

    my $parentInterfaceName = ($pureInterface) ? "IUnknown" : GetParentInterface($dataNode);

    my $numConstants = @{$dataNode->constants};
    my $numAttributes = @{$dataNode->attributes};
    my $numFunctions = @{$dataNode->functions};

    # - Add default header template
    @IDLHeader = @licenseTemplate;
    push(@IDLHeader, "\n");

    # - INCLUDES -
    push(@IDLHeader, "#ifndef DO_NO_IMPORTS\n");
    push(@IDLHeader, "import \"oaidl.idl\";\n");
    push(@IDLHeader, "import \"ocidl.idl\";\n");
    push(@IDLHeader, "#endif\n\n");

    unless ($pureInterface) {
        push(@IDLHeader, "#ifndef DO_NO_IMPORTS\n");
        push(@IDLHeader, "import \"${parentInterfaceName}.idl\";\n");
        push(@IDLHeader, "#endif\n\n");

        $IDLDontForwardDeclare{$outInterfaceName} = 1;
        $IDLDontImport{$outInterfaceName} = 1;
        $IDLDontForwardDeclare{$parentInterfaceName} = 1;
        $IDLDontImport{$parentInterfaceName} = 1;
    }

    # - Begin
    # -- Attributes
    push(@IDLContent, "[\n");
    push(@IDLContent, "    object,\n");
    push(@IDLContent, "    oleautomation,\n");
    push(@IDLContent, "    uuid(" . $uuid . "),\n");
    push(@IDLContent, "    pointer_default(unique)\n");
    push(@IDLContent, "]\n");

    # -- Interface
    push(@IDLContent, "interface " . $outInterfaceName . " : " . $parentInterfaceName . "\n");
    push(@IDLContent, "{\n");


    # - FIXME: Add constants.


    # - Add attribute getters/setters.
    if ($numAttributes > 0) {
        foreach my $attribute (@{$dataNode->attributes}) {
            my $attributeName = $attribute->signature->name;
            my $attributeIDLType = $attribute->signature->type;
            my $attributeTypeIn = GetCOMTypeIn($attributeIDLType);
            my $attributeTypeOut = GetCOMTypeOut($attributeIDLType);
            my $attributeIsReadonly = ($attribute->type =~ /^readonly/);

            AddForwardDeclarationsForTypeInIDL($attributeIDLType);

            unless ($attributeIsReadonly) {
                # Setter
                my $setterName = "set" . $codeGenerator->WK_ucfirst($attributeName);
                my $setter = "    HRESULT " . $setterName . "([in] " . $attributeTypeIn . ");\n";
                push(@IDLContent, $setter);
            }

            # Getter
            my $getter = "    HRESULT " . $attributeName . "([out, retval] " . $attributeTypeOut . "*);\n\n";
            push(@IDLContent, $getter);
        }
    }

    # - Add functions.
    if ($numFunctions > 0) {
        foreach my $function (@{$dataNode->functions}) {
            my $functionName = $function->signature->name;
            my $returnIDLType = $function->signature->type;
            my $returnType = GetCOMTypeOut($returnIDLType);
            my $noReturn = ($returnType eq "void");

            AddForwardDeclarationsForTypeInIDL($returnIDLType);

            my @paramArgList = ();
            foreach my $param (@{$function->parameters}) {
                my $paramName = $param->name;
                my $paramIDLType = $param->type;
                my $paramType = GetCOMTypeIn($param->type);

                AddForwardDeclarationsForTypeInIDL($paramIDLType);

                # Form parameter
                my $parameter = "[in] ${paramType} ${paramName}";

                # Add parameter to function signature
                push(@paramArgList, $parameter);
            }

            unless ($noReturn) {
                my $resultParameter = "[out, retval] " . $returnType . "* result";
                push(@paramArgList, $resultParameter);
            }

            my $functionSig = "    HRESULT " . $functionName . "(";
            $functionSig .= join(", ", @paramArgList);
            $functionSig .= ");\n\n";
            push(@IDLContent, $functionSig);
        }
    }

    # - End
    push(@IDLContent, "}\n\n");
}

sub GenerateInterfaceHeader
{
    my ($object, $dataNode) = @_;

    my $IDLType = $dataNode->name;
    my $implementationClass = IDLTypeToImplementationType($IDLType);
    my $implementationClassWithoutNamespace = StripNamespace($implementationClass);
    my $className = GetClassName($IDLType);
    my $interfaceName = GetInterfaceName($IDLType);

    # - Add default header template
    @CPPInterfaceHeader = @licenseTemplate;
    push(@CPPInterfaceHeader, "\n");

    # - Header guards -
    push(@CPPInterfaceHeader, "#ifndef " . $className . "_h\n");
    push(@CPPInterfaceHeader, "#define " . $className . "_h\n\n");

    # - Forward Declarations -
    push(@CPPInterfaceHeader, "interface ${interfaceName};\n\n");
    push(@CPPInterfaceHeader, "namespace WebCore {\n");
    push(@CPPInterfaceHeader, "    class ${implementationClassWithoutNamespace};\n");
    push(@CPPInterfaceHeader, "}\n\n");

    # - Default Interface Creator -
    push(@CPPInterfaceHeader, "${interfaceName}* to${interfaceName}(${implementationClass}*) { return 0; }\n\n");

    push(@CPPInterfaceHeader, "#endif // " . $className . "_h\n");
}

# -----------------------------------------------------------------------------
#    CPP Helper Functions
# -----------------------------------------------------------------------------

sub GenerateCPPAttributeSignature
{
    my ($attribute, $className, $options) = @_;

    my $attributeName = $attribute->signature->name;
    my $isReadonly = ($attribute->type =~ /^readonly/);

    my $newline = $$options{"NewLines"} ? "\n" : "";
    my $indent = $$options{"Indent"} ? " " x $$options{"Indent"} : "";
    my $semicolon = $$options{"IncludeSemiColon"} ? ";" : "";
    my $virtual = $$options{"AddVirtualKeyword"} ? "virtual " : "";
    my $class = $$options{"UseClassName"} ? "${className}::" : "";
    my $forwarder = $$options{"Forwarder"} ? 1 : 0;
    my $joiner =  ($$options{"NewLines"} ? "\n" . $indent . "    " : "");

    my %attributeSignatures = ();

    unless ($isReadonly) {
        my $attributeTypeIn = GetCOMTypeIn($attribute->signature->type);
        my $setterName = "set" . $codeGenerator->WK_ucfirst($attributeName);
        my $setter = $indent . $virtual . "HRESULT STDMETHODCALLTYPE ". $class . $setterName . "(";
        $setter .= $joiner . "/* [in] */ ${attributeTypeIn} ${attributeName})" . $semicolon . $newline;
        if ($forwarder) {
            $setter .= " { return " . $$options{"Forwarder"} . "::" . $setterName . "(${attributeName}); }\n";
        }
        $attributeSignatures{"Setter"} = $setter;
    }

    my $attributeTypeOut = GetCOMTypeOut($attribute->signature->type);
    my $getter = $indent . $virtual . "HRESULT STDMETHODCALLTYPE " . $class . $attributeName . "(";
    $getter .= $joiner . "/* [retval][out] */ ${attributeTypeOut}* result)" . $semicolon . $newline;
    if ($forwarder) {
        $getter .= " { return " . $$options{"Forwarder"} . "::" . $attributeName . "(result); }\n";
    }
    $attributeSignatures{"Getter"} = $getter;

    return %attributeSignatures;
}


sub GenerateCPPAttribute
{
    my ($attribute, $className, $implementationClass) = @_;

    my $implementationClassWithoutNamespace = StripNamespace($implementationClass);

    my $attributeName = $attribute->signature->name;
    my $attributeIDLType = $attribute->signature->type;
    my $hasSetterException = @{$attribute->setterExceptions};
    my $hasGetterException = @{$attribute->getterExceptions};
    my $isReadonly = ($attribute->type =~ /^readonly/);
    my $attributeTypeIsPrimitive = $codeGenerator->IsPrimitiveType($attributeIDLType);
    my $attributeTypeIsString = $codeGenerator->IsStringType($attributeIDLType);
    my $attributeImplementationType = IDLTypeToImplementationType($attributeIDLType);
    my $attributeImplementationTypeWithoutNamespace = StripNamespace($attributeImplementationType);
    my $attributeTypeCOMClassName = GetClassName($attributeIDLType);

    $CPPImplementationWebCoreIncludes{"ExceptionCode.h"} = 1 if $hasSetterException or $hasGetterException;

    my %signatures = GenerateCPPAttributeSignature($attribute, $className, { "NewLines" => 1,
                                                                             "Indent" => 0,
                                                                             "IncludeSemiColon" => 0,
                                                                             "UseClassName" => 1,
                                                                             "AddVirtualKeyword" => 0 });

    my %attrbutesToReturn = ();

    unless ($isReadonly) {
        my @setterImplementation = ();
        push(@setterImplementation, $signatures{"Setter"});
        push(@setterImplementation, "{\n");

        my $setterName = "set" . $codeGenerator->WK_ucfirst($attributeName);

        my @setterParams = ();
        if ($attributeTypeIsString) {
            push(@setterParams, $attributeName);
            if ($hasSetterException) {
                push(@setterImplementation, "    WebCore::ExceptionCode ec = 0;\n");
                push(@setterParams, "ec");
            }
        } elsif ($attributeTypeIsPrimitive) {
            if ($attribute->signature->extendedAttributes->{"ConvertFromString"}) {
                push(@setterParams, "WebCore::String::number(${attributeName})");
            } elsif ($attributeIDLType eq "boolean") {
                push(@setterParams, "!!${attributeName}");
            } else {
                my $primitiveImplementationType = IDLTypeToImplementationType($attributeIDLType);
                push(@setterParams, "static_cast<${primitiveImplementationType}>(${attributeName})");
            }

            if ($hasSetterException) {
                push(@setterImplementation, "    WebCore::ExceptionCode ec = 0;\n");
                push(@setterParams, "ec");
            }
        } else {
            $CPPImplementationWebCoreIncludes{"COMPtr.h"} = 1;

            push(@setterImplementation, "    if (!${attributeName})\n");
            push(@setterImplementation, "        return E_POINTER;\n\n");
            push(@setterImplementation, "    COMPtr<${attributeTypeCOMClassName}> ptr(Query, ${attributeName});\n");
            push(@setterImplementation, "    if (!ptr)\n");
            push(@setterImplementation, "        return E_NOINTERFACE;\n");

            push(@setterParams, "ptr->impl${attributeImplementationTypeWithoutNamespace}()");
            if ($hasSetterException) {
                push(@setterImplementation, "    WebCore::ExceptionCode ec = 0;\n");
                push(@setterParams, "ec");
            }
        }

        # FIXME: CHECK EXCEPTION AND DO SOMETHING WITH IT

        my $reflect = $attribute->signature->extendedAttributes->{"Reflect"};
        my $reflectURL = $attribute->signature->extendedAttributes->{"ReflectURL"};
        if ($reflect || $reflectURL) {
            $CPPImplementationWebCoreIncludes{"HTMLNames.h"} = 1;
            my $contentAttributeName = (($reflect || $reflectURL) eq "1") ? $attributeName : ($reflect || $reflectURL);
            push(@setterImplementation, "    impl${implementationClassWithoutNamespace}()->setAttribute(WebCore::HTMLNames::${contentAttributeName}Attr, " . join(", ", @setterParams) . ");\n");
        } else {
            push(@setterImplementation, "    impl${implementationClassWithoutNamespace}()->${setterName}(" . join(", ", @setterParams) . ");\n");
        }
        push(@setterImplementation, "    return S_OK;\n");
        push(@setterImplementation, "}\n\n");

        $attrbutesToReturn{"Setter"} = join("", @setterImplementation);
    }

    my @getterImplementation = ();
    push(@getterImplementation, $signatures{"Getter"});
    push(@getterImplementation, "{\n");
    push(@getterImplementation, "    if (!result)\n");
    push(@getterImplementation, "        return E_POINTER;\n\n");

    my $implementationGetter;
    my $reflect = $attribute->signature->extendedAttributes->{"Reflect"};
    my $reflectURL = $attribute->signature->extendedAttributes->{"ReflectURL"};
    if ($reflect || $reflectURL) {
        $implIncludes{"HTMLNames.h"} = 1;
        my $contentAttributeName = (($reflect || $reflectURL) eq "1") ? $attributeName : ($reflect || $reflectURL);
        my $getAttributeFunctionName = $reflectURL ? "getURLAttribute" : "getAttribute";
        $implementationGetter = "impl${implementationClassWithoutNamespace}()->${getAttributeFunctionName}(WebCore::HTMLNames::${contentAttributeName}Attr)";
    } else {
        $implementationGetter = "impl${implementationClassWithoutNamespace}()->" . $codeGenerator->WK_lcfirst($attributeName) . "(" . ($hasGetterException ? "ec" : ""). ")";
    }

    push(@getterImplementation, "    WebCore::ExceptionCode ec = 0;\n") if $hasGetterException;

    if ($attributeTypeIsString) {
        push(@getterImplementation, "    *result = WebCore::BString(${implementationGetter}).release();\n");
    } elsif ($attributeTypeIsPrimitive) {
        if ($attribute->signature->extendedAttributes->{"ConvertFromString"}) {
            push(@getterImplementation, "    *result = static_cast<${attributeTypeCOMClassName}>(${implementationGetter}.toInt());\n");
        } else {
            push(@getterImplementation, "    *result = static_cast<${attributeTypeCOMClassName}>(${implementationGetter});\n");
        }
    } else {
        $CPPImplementationIncludesAngle{"wtf/GetPtr.h"} = 1;
        my $attributeTypeCOMInterfaceName = GetInterfaceName($attributeIDLType);
        push(@getterImplementation, "    *result = 0;\n");
        push(@getterImplementation, "    ${attributeImplementationType}* resultImpl = WTF::getPtr(${implementationGetter});\n");
        push(@getterImplementation, "    if (!resultImpl)\n");
        push(@getterImplementation, "        return E_POINTER;\n\n");
        push(@getterImplementation, "    *result = to${attributeTypeCOMInterfaceName}(resultImpl);\n");
    }

    # FIXME: CHECK EXCEPTION AND DO SOMETHING WITH IT

    push(@getterImplementation, "    return S_OK;\n");
    push(@getterImplementation, "}\n\n");

    $attrbutesToReturn{"Getter"} = join("", @getterImplementation);

    return %attrbutesToReturn;
}

sub GenerateCPPFunctionSignature
{
    my ($function, $className, $options) = @_;

    my $functionName = $function->signature->name;
    my $returnIDLType = $function->signature->type;
    my $returnType = GetCOMTypeOut($returnIDLType);
    my $noReturn = ($returnType eq "void");

    my $newline = $$options{"NewLines"} ? "\n" : "";
    my $indent = $$options{"Indent"} ? " " x $$options{"Indent"} : "";
    my $semicolon = $$options{"IncludeSemiColon"} ? ";" : "";
    my $virtual = $$options{"AddVirtualKeyword"} ? "virtual " : "";
    my $class = $$options{"UseClassName"} ? "${className}::" : "";
    my $forwarder = $$options{"Forwarder"} ? 1 : 0;
    my $joiner = ($$options{"NewLines"} ? "\n" . $indent . "    " : " ");

    my @paramArgList = ();
    foreach my $param (@{$function->parameters}) {
        my $paramName = $param->name;
        my $paramType = GetCOMTypeIn($param->type);
        my $parameter = "/* [in] */ ${paramType} ${paramName}";
        push(@paramArgList, $parameter);
    }

    unless ($noReturn) {
        my $resultParameter .= "/* [out, retval] */ ${returnType}* result";
        push(@paramArgList, $resultParameter);
    }

    my $functionSig = $indent . $virtual . "HRESULT STDMETHODCALLTYPE " . $class . $functionName . "(";
    $functionSig .= $joiner . join("," . $joiner, @paramArgList) if @paramArgList > 0;
    $functionSig .= ")" . $semicolon . $newline;
    if ($forwarder) {
        my @paramNameList = ();
        push(@paramNameList, $_->name) foreach (@{$function->parameters});
        push(@paramNameList, "result") unless $noReturn;
        $functionSig .= " { return " . $$options{"Forwarder"} . "::" . $functionName . "(" . join(", ", @paramNameList) . "); }\n";
    }

    return $functionSig
}

sub GenerateCPPFunction
{
    my ($function, $className, $implementationClass) = @_;

    my @functionImplementation = ();

    my $signature = GenerateCPPFunctionSignature($function, $className, { "NewLines" => 1,
                                                                          "Indent" => 0,
                                                                          "IncludeSemiColon" => 0,
                                                                          "UseClassName" => 1,
                                                                          "AddVirtualKeyword" => 0 });

    my $implementationClassWithoutNamespace = StripNamespace($implementationClass);

    my $functionName = $function->signature->name;
    my $returnIDLType = $function->signature->type;
    my $noReturn = ($returnIDLType eq "void");
    my $raisesExceptions = @{$function->raisesExceptions};

    AddIncludesForTypeInCPPImplementation($returnIDLType);
    $CPPImplementationWebCoreIncludes{"ExceptionCode.h"} = 1 if $raisesExceptions;

    my %needsCustom = ();
    my @parameterInitialization = ();
    my @parameterList = ();
    foreach my $param (@{$function->parameters}) {
        my $paramName = $param->name;
        my $paramIDLType = $param->type;

        my $paramTypeIsPrimitive = $codeGenerator->IsPrimitiveType($paramIDLType);
        my $paramTypeIsString = $codeGenerator->IsStringType($paramIDLType);

        $needsCustom{"NodeToReturn"} = $paramName if $param->extendedAttributes->{"Return"};

        AddIncludesForTypeInCPPImplementation($paramIDLType);

        # FIXME: We may need to null check the arguments as well

        if ($paramTypeIsString) {
            push(@parameterList, $paramName);
        } elsif ($paramTypeIsPrimitive) {
            if ($paramIDLType eq "boolean") {
                push(@parameterList, "!!${paramName}");
            } else {
                my $primitiveImplementationType = IDLTypeToImplementationType($paramIDLType);
                push(@parameterList, "static_cast<${primitiveImplementationType}>(${paramName})");
            }
        } else {
            $CPPImplementationWebCoreIncludes{"COMPtr.h"} = 1;

            $needsCustom{"CanReturnEarly"} = 1;

            my $paramTypeCOMClassName = GetClassName($paramIDLType);
            my $paramTypeImplementationWithoutNamespace = StripNamespace(IDLTypeToImplementationType($paramIDLType));
            my $ptrName = "ptrFor" . $codeGenerator->WK_ucfirst($paramName);
            my $paramInit = "    COMPtr<${paramTypeCOMClassName}> ${ptrName}(Query, ${paramName});\n";
            $paramInit .= "    if (!${ptrName})\n";
            $paramInit .= "        return E_NOINTERFACE;";
            push(@parameterInitialization, $paramInit);
            push(@parameterList, "${ptrName}->impl${paramTypeImplementationWithoutNamespace}()");
        }
    }

    push(@parameterList, "ec") if $raisesExceptions;

    my $implementationGetter = "impl${implementationClassWithoutNamespace}()";

    my $callSigBegin = "    ";
    my $callSigMiddle = "${implementationGetter}->" . $codeGenerator->WK_lcfirst($functionName) . "(" . join(", ", @parameterList) . ")";
    my $callSigEnd = ";\n";

    if (defined $needsCustom{"NodeToReturn"}) {
        my $nodeToReturn = $needsCustom{"NodeToReturn"};
        $callSigBegin .= "if (";
        $callSigEnd = ")\n";
        $callSigEnd .= "        *result = ${nodeToReturn};";
    } elsif (!$noReturn) {
        my $returnTypeIsString =  $codeGenerator->IsStringType($returnIDLType);
        my $returnTypeIsPrimitive = $codeGenerator->IsPrimitiveType($returnIDLType);

        if ($returnTypeIsString) {
            $callSigBegin .= "*result = WebCore::BString(";
            $callSigEnd = ").release();\n";
        } elsif ($returnTypeIsPrimitive) {
            my $primitiveCOMType = GetClassName($returnIDLType);
            $callSigBegin .= "*result = static_cast<${primitiveCOMType}>(";
            $callSigEnd = ");";
        } else {
            $CPPImplementationIncludesAngle{"wtf/GetPtr.h"} = 1;
            my $returnImplementationType = IDLTypeToImplementationType($returnIDLType);
            my $returnTypeCOMInterfaceName = GetInterfaceName($returnIDLType);
            $callSigBegin .= "${returnImplementationType}* resultImpl = WTF::getPtr(";
            $callSigEnd = ");\n";
            $callSigEnd .= "    if (!resultImpl)\n";
            $callSigEnd .= "        return E_POINTER;\n\n";
            $callSigEnd .= "    *result = to${returnTypeCOMInterfaceName}(resultImpl);";
        }
    }

    push(@functionImplementation, $signature);
    push(@functionImplementation, "{\n");
    unless ($noReturn) {
        push(@functionImplementation, "    if (!result)\n");
        push(@functionImplementation, "        return E_POINTER;\n\n");
        push(@functionImplementation, "    *result = 0;\n\n") if $needsCustom{"CanReturnEarly"};
    }
    push(@functionImplementation, "    WebCore::ExceptionCode ec = 0;\n") if $raisesExceptions; # FIXME: CHECK EXCEPTION AND DO SOMETHING WITH IT
    push(@functionImplementation, join("\n", @parameterInitialization) . (@parameterInitialization > 0 ? "\n" : ""));
    push(@functionImplementation, $callSigBegin . $callSigMiddle . $callSigEnd . "\n");
    push(@functionImplementation, "    return S_OK;\n");
    push(@functionImplementation, "}\n\n");

    return join("", @functionImplementation);
}


# -----------------------------------------------------------------------------
#    CPP Header
# -----------------------------------------------------------------------------

sub GenerateCPPHeader
{
    my ($object, $dataNode) = @_;

    my $IDLType = $dataNode->name;
    my $implementationClass = IDLTypeToImplementationType($IDLType);
    my $implementationClassWithoutNamespace = StripNamespace($implementationClass);
    my $className = GetClassName($IDLType);
    my $interfaceName = GetInterfaceName($IDLType);

    my $parentClassName = GetParentClass($dataNode);
    my @otherInterfacesImplemented = GetAdditionalInterfaces($IDLType);
    foreach my $otherInterface (@otherInterfacesImplemented) {
        push(@additionalInterfaceDefinitions, $codeGenerator->ParseInterface($otherInterface));
    }

    # FIXME: strip whitespace from UUID
    my $uuid = $dataNode->extendedAttributes->{"ImplementationUUID"} || die "All classes require an ImplementationUUID extended attribute.";

    my $numAttributes = @{$dataNode->attributes};
    my $numFunctions = @{$dataNode->functions};

    # - Add default header template
    @CPPHeaderHeader = @licenseTemplate;
    push(@CPPHeaderHeader, "\n");

    # - Header guards -
    push(@CPPHeaderHeader, "#ifndef " . $className . "_h\n");
    push(@CPPHeaderHeader, "#define " . $className . "_h\n\n");

    AddIncludesForTypeInCPPHeader($interfaceName);
    AddIncludesForTypeInCPPHeader($parentClassName);
    $CPPHeaderDontForwardDeclarations{$className} = 1;
    $CPPHeaderDontForwardDeclarations{$interfaceName} = 1;
    $CPPHeaderDontForwardDeclarations{$parentClassName} = 1;

    # -- Forward declare implementation type
    push(@CPPHeaderContent, "namespace WebCore {\n");
    push(@CPPHeaderContent, "    class ". StripNamespace($implementationClass) . ";\n");
    push(@CPPHeaderContent, "}\n\n");

    # -- Start Class --
    my @parentsClasses = ($parentClassName, $interfaceName);
    push(@parentsClasses, map { GetInterfaceName($_) } @otherInterfacesImplemented);
    push(@CPPHeaderContent, "class __declspec(uuid(\"$uuid\")) ${className} : " . join(", ", map { "public $_" } @parentsClasses) . " {\n");

    # Add includes for all additional interfaces to implement
    map { AddIncludesForTypeInCPPHeader(GetInterfaceName($_)) } @otherInterfacesImplemented;

    # -- BASICS --
    # FIXME: The constructor and destructor should be protected, but the current design of
    # createInstance requires them to be public.  One solution is to friend the constructor
    # of the top-level-class with every one of its child classes, but that requires information
    # this script currently does not have, though possibly could determine.
    push(@CPPHeaderContent, "public:\n");
    push(@CPPHeaderContent, "   ${className}(${implementationClass}*);\n");
    push(@CPPHeaderContent, "   virtual ~${className}();\n\n");

    push(@CPPHeaderContent, "public:\n");
    push(@CPPHeaderContent, "    static ${className}* createInstance(${implementationClass}*);\n\n");

    push(@CPPHeaderContent, "    // IUnknown\n");
    push(@CPPHeaderContent, "    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** ppvObject);\n");
    push(@CPPHeaderContent, "    virtual ULONG STDMETHODCALLTYPE AddRef() { return ${parentClassName}::AddRef(); }\n");
    push(@CPPHeaderContent, "    virtual ULONG STDMETHODCALLTYPE Release() { return ${parentClassName}::Release(); }\n\n");


    # -- Parent Class Forwards --
    if (@{$dataNode->parents}) {
        my %attributeNameSet = map {($_->signature->name, 1)} @{$dataNode->attributes};
        my %functionNameSet = map {($_->signature->name, 1)} @{$dataNode->functions};

        my @parentLists = $codeGenerator->GetMethodsAndAttributesFromParentClasses($dataNode);
        push(@CPPHeaderContent, "\n");
        foreach my $parentHash (@parentLists) {

            push(@CPPHeaderContent, "    // " . GetInterfaceName($parentHash->{'name'}) . $DASHES . "\n");

            my @attributeList = @{$parentHash->{'attributes'}};
            push(@CPPHeaderContent, "    // Attributes\n");
            foreach my $attribute (@attributeList) {
                # Don't forward an attribute that this class redefines.
                next if $attributeNameSet{$attribute->signature->name};

                AddForwardDeclarationsForTypeInCPPHeader($attribute->signature->type);

                my %attributes = GenerateCPPAttributeSignature($attribute, $className, { "NewLines" => 0,
                                                                                         "Indent" => 4,
                                                                                         "IncludeSemiColon" => 0,
                                                                                         "AddVirtualKeyword" => 1,
                                                                                         "UseClassName" => 0,
                                                                                         "Forwarder" => $parentClassName });
                push(@CPPHeaderContent, values(%attributes));
            }

            # Add attribute names to attribute names set in case other ancestors 
            # also define them.
            $attributeNameSet{$_->signature->name} = 1 foreach @attributeList;

            push(@CPPHeaderContent, "\n");

            my @functionList = @{$parentHash->{'functions'}};
            push(@CPPHeaderContent, "    // Functions\n");
            foreach my $function (@functionList) {
                # Don't forward a function that this class redefines.
                next if $functionNameSet{$function->signature->name};

                AddForwardDeclarationsForTypeInCPPHeader($function->signature->type);
                AddForwardDeclarationsForTypeInCPPHeader($_->type) foreach (@{$function->parameters});

                my $functionSig = GenerateCPPFunctionSignature($function, $className, { "NewLines" => 0,
                                                                                        "Indent" => 4,
                                                                                        "IncludeSemiColon" => 0,
                                                                                        "AddVirtualKeyword" => 1,
                                                                                        "UseClassName" => 0,
                                                                                        "Forwarder" => $parentClassName });

                push(@CPPHeaderContent, $functionSig);
            }
            # Add functions names to functions names set in case other ancestors 
            # also define them.
            $functionNameSet{$_->signature->name} = 1 foreach @functionList;

            push(@CPPHeaderContent, "\n");
        }
    }

    # - Additional interfaces to implement -
    foreach my $interfaceToImplement (@additionalInterfaceDefinitions) {
        my $IDLTypeOfInterfaceToImplement = $interfaceToImplement->name;
        my $nameOfInterfaceToImplement = GetInterfaceName($IDLTypeOfInterfaceToImplement);
        my $numAttributesInInterface = @{$interfaceToImplement->attributes};
        my $numFunctionsInInterface = @{$interfaceToImplement->functions};

        push(@CPPHeaderContent, "    // ${nameOfInterfaceToImplement} ${DASHES}\n\n");

        # - Add attribute getters/setters.
        if ($numAttributesInInterface > 0) {
            push(@CPPHeaderContent, "    // Attributes\n\n");
            foreach my $attribute (@{$interfaceToImplement->attributes}) {
                AddForwardDeclarationsForTypeInCPPHeader($attribute->signature->type);

                my %attributeSigs = GenerateCPPAttributeSignature($attribute, $className, { "NewLines" => 1,
                                                                                            "Indent" => 4,
                                                                                            "IncludeSemiColon" => 1,
                                                                                            "AddVirtualKeyword" => 1,
                                                                                            "UseClassName" => 0 });

                push(@CPPHeaderContent, values(%attributeSigs));
                push(@CPPHeaderContent, "\n");
            }
        }

        # - Add functions.
        if ($numFunctionsInInterface > 0) {
            push(@CPPHeaderContent, "    // Functions\n\n");
            foreach my $function (@{$interfaceToImplement->functions}) {
                AddForwardDeclarationsForTypeInCPPHeader($function->signature->type);
                AddForwardDeclarationsForTypeInCPPHeader($_->type) foreach (@{$function->parameters});

                my $functionSig = GenerateCPPFunctionSignature($function, $className, { "NewLines" => 1,
                                                                                        "Indent" => 4,
                                                                                        "IncludeSemiColon" => 1,
                                                                                        "AddVirtualKeyword" => 1,
                                                                                        "UseClassName" => 0 });

                push(@CPPHeaderContent, $functionSig);
                push(@CPPHeaderContent, "\n");
            }
        }
    }

    if ($numAttributes > 0 || $numFunctions > 0) {
        push(@CPPHeaderContent, "    // ${interfaceName} ${DASHES}\n\n");
    }

    # - Add constants COMING SOON

    # - Add attribute getters/setters.
    if ($numAttributes > 0) {
        push(@CPPHeaderContent, "    // Attributes\n\n");
        foreach my $attribute (@{$dataNode->attributes}) {
            AddForwardDeclarationsForTypeInCPPHeader($attribute->signature->type);

            my %attributeSigs = GenerateCPPAttributeSignature($attribute, $className, { "NewLines" => 1,
                                                                                        "Indent" => 4,
                                                                                        "IncludeSemiColon" => 1,
                                                                                        "AddVirtualKeyword" => 1,
                                                                                        "UseClassName" => 0 });

            push(@CPPHeaderContent, values(%attributeSigs));
            push(@CPPHeaderContent, "\n");
        }
    }

    # - Add functions.
    if ($numFunctions > 0) {
        push(@CPPHeaderContent, "    // Functions\n\n");
        foreach my $function (@{$dataNode->functions}) {
            AddForwardDeclarationsForTypeInCPPHeader($function->signature->type);
            AddForwardDeclarationsForTypeInCPPHeader($_->type) foreach (@{$function->parameters});

            my $functionSig = GenerateCPPFunctionSignature($function, $className, { "NewLines" => 1,
                                                                                    "Indent" => 4,
                                                                                    "IncludeSemiColon" => 1,
                                                                                    "AddVirtualKeyword" => 1,
                                                                                    "UseClassName" => 0 });

            push(@CPPHeaderContent, $functionSig);
            push(@CPPHeaderContent, "\n");
        }
    }

    push(@CPPHeaderContent, "    ${implementationClass}* impl${implementationClassWithoutNamespace}() const;\n");

    if (@{$dataNode->parents} == 0) {
        AddIncludesForTypeInCPPHeader("wtf/RefPtr", 1);
        push(@CPPHeaderContent, "\n");
        push(@CPPHeaderContent, "    ${implementationClass}* impl() const { return m_impl.get(); }\n\n");
        push(@CPPHeaderContent, "private:\n");
        push(@CPPHeaderContent, "    RefPtr<${implementationClass}> m_impl;\n");
    }

    # -- End Class --
    push(@CPPHeaderContent, "};\n\n");

    # -- Default Interface Creator --
    push(@CPPHeaderContent, "${interfaceName}* to${interfaceName}(${implementationClass}*);\n\n");

    push(@CPPHeaderContent, "#endif // " . $className . "_h\n");
}


# -----------------------------------------------------------------------------
#    CPP Implementation
# -----------------------------------------------------------------------------

sub GenerateCPPImplementation
{
    my ($object, $dataNode) = @_;

    my $IDLType = $dataNode->name;
    my $implementationClass = IDLTypeToImplementationType($IDLType);
    my $implementationClassWithoutNamespace = StripNamespace($implementationClass);
    my $className = GetClassName($IDLType);
    my $interfaceName = GetInterfaceName($IDLType);

    my $parentClassName = GetParentClass($dataNode);
    my $isBaseClass = (@{$dataNode->parents} == 0);

    my $uuid = $dataNode->extendedAttributes->{"ImplementationUUID"} || die "All classes require an ImplementationUUID extended attribute.";

    my $numAttributes = @{$dataNode->attributes};
    my $numFunctions = @{$dataNode->functions};

    # - Add default header template
    @CPPImplementationHeader = @licenseTemplate;
    push(@CPPImplementationHeader, "\n");

    push(@CPPImplementationHeader, "#include \"config.h\"\n");
    push(@CPPImplementationHeader, "#include \"WebKitDLL.h\"\n");
    push(@CPPImplementationHeader, "#include " . ($className eq "GEN_DOMImplementation" ? "\"GEN_DOMDOMImplementation.h\"" : "\"${className}.h\"") . "\n");
    $CPPImplementationDontIncludes{"${className}.h"} = 1;
    $CPPImplementationWebCoreIncludes{"${implementationClassWithoutNamespace}.h"} = 1;

    # -- Constructor --
    push(@CPPImplementationContent, "${className}::${className}(${implementationClass}* impl)\n");
    if ($isBaseClass) {
        push(@CPPImplementationContent, "    : m_impl(impl)\n");
        push(@CPPImplementationContent, "{\n");
        push(@CPPImplementationContent, "    ASSERT_ARG(impl, impl);\n");
        push(@CPPImplementationContent, "}\n\n");
    } else {
        push(@CPPImplementationContent, "    : ${parentClassName}(impl)\n");
        push(@CPPImplementationContent, "{\n");
        push(@CPPImplementationContent, "}\n\n");
    }

    # -- Destructor --
    push(@CPPImplementationContent, "${className}::~${className}()\n");
    push(@CPPImplementationContent, "{\n");
    if ($isBaseClass) {
        $CPPImplementationIncludes{"DOMCreateInstance.h"} = 1;
        push(@CPPImplementationContent, "    removeDOMWrapper(impl());\n");
    }
    push(@CPPImplementationContent, "}\n\n");

    push(@CPPImplementationContent, "${implementationClass}* ${className}::impl${implementationClassWithoutNamespace}() const\n");
    push(@CPPImplementationContent, "{\n");
    push(@CPPImplementationContent, "    return static_cast<${implementationClass}*>(impl());\n");
    push(@CPPImplementationContent, "}\n\n");

    # Base classes must implement the createInstance method externally.
    if (@{$dataNode->parents} != 0) {
        push(@CPPImplementationContent, "${className}* ${className}::createInstance(${implementationClass}* impl)\n");
        push(@CPPImplementationContent, "{\n");
        push(@CPPImplementationContent, "    return static_cast<${className}*>(${parentClassName}::createInstance(impl));\n");
        push(@CPPImplementationContent, "}\n");
    }

    push(@CPPImplementationContent, "// IUnknown $DASHES\n\n");

    # -- QueryInterface --
    push(@CPPImplementationContent, "HRESULT STDMETHODCALLTYPE ${className}::QueryInterface(REFIID riid, void** ppvObject)\n");
    push(@CPPImplementationContent, "{\n");
    push(@CPPImplementationContent, "    *ppvObject = 0;\n");
    push(@CPPImplementationContent, "    if (IsEqualGUID(riid, IID_${interfaceName}))\n");
    push(@CPPImplementationContent, "        *ppvObject = reinterpret_cast<${interfaceName}*>(this);\n");
    push(@CPPImplementationContent, "    else if (IsEqualGUID(riid, __uuidof(${className})))\n");
    push(@CPPImplementationContent, "        *ppvObject = reinterpret_cast<${className}*>(this);\n");
    push(@CPPImplementationContent, "    else\n");
    push(@CPPImplementationContent, "        return ${parentClassName}::QueryInterface(riid, ppvObject);\n\n");
    push(@CPPImplementationContent, "    AddRef();\n");
    push(@CPPImplementationContent, "    return S_OK;\n");
    push(@CPPImplementationContent, "}\n\n");

    # - Additional interfaces to implement -
    foreach my $interfaceToImplement (@additionalInterfaceDefinitions) {
        my $IDLTypeOfInterfaceToImplement = $interfaceToImplement->name;
        my $nameOfInterfaceToImplement = GetInterfaceName($IDLTypeOfInterfaceToImplement);
        my $numAttributesInInterface = @{$interfaceToImplement->attributes};
        my $numFunctionsInInterface = @{$interfaceToImplement->functions};

        push(@CPPImplementationContent, "    // ${nameOfInterfaceToImplement} ${DASHES}\n\n");

        if ($numAttributesInInterface > 0) {
            push(@CPPImplementationContent, "// Attributes\n\n");
            foreach my $attribute (@{$interfaceToImplement->attributes}) {
                # FIXME: Do this in one step.
                # FIXME: Implement exception handling.

                AddIncludesForTypeInCPPImplementation($attribute->signature->type);

                my %attributes = GenerateCPPAttribute($attribute, $className, $implementationClass);
                push(@CPPImplementationContent, values(%attributes));
            }
        }

        # - Add functions.
        if ($numFunctionsInInterface > 0) {
            push(@CPPImplementationContent, "// Functions\n\n");

            foreach my $function (@{$interfaceToImplement->functions}) {
                my $functionImplementation = GenerateCPPFunction($function, $className, $implementationClass);
                push(@CPPImplementationContent, $functionImplementation);
            }
        }
    }

    push(@CPPImplementationContent, "// ${interfaceName} $DASHES\n\n");

    # - Add attribute getters/setters.
    if ($numAttributes > 0) {
        push(@CPPImplementationContent, "// Attributes\n\n");
        foreach my $attribute (@{$dataNode->attributes}) {
            # FIXME: do this in one step
            my $hasSetterException = @{$attribute->setterExceptions};
            my $hasGetterException = @{$attribute->getterExceptions};

            AddIncludesForTypeInCPPImplementation($attribute->signature->type);

            my %attributes = GenerateCPPAttribute($attribute, $className, $implementationClass);
            push(@CPPImplementationContent, values(%attributes));
        }
    }

    # - Add functions.
    if ($numFunctions > 0) {
        push(@CPPImplementationContent, "// Functions\n\n");

        foreach my $function (@{$dataNode->functions}) {
            my $functionImplementation = GenerateCPPFunction($function, $className, $implementationClass);
            push(@CPPImplementationContent, $functionImplementation);
        }
    }

    # - Default implementation for interface creator.
    # FIXME: add extended attribute to add custom implementation if necessary.
    push(@CPPImplementationContent, "${interfaceName}* to${interfaceName}(${implementationClass}* impl)\n");
    push(@CPPImplementationContent, "{\n");
    push(@CPPImplementationContent, "    return ${className}::createInstance(impl);\n");
    push(@CPPImplementationContent, "}\n");
}

sub WriteData
{
    my ($object, $name, $pureInterface) = @_;

    # -- IDL --
    my $IDLFileName = "$outputDir/I" . $TEMP_PREFIX . "DOM" . $name . ".idl";
    unlink($IDLFileName);

    # Write to output IDL.
    open(OUTPUTIDL, ">$IDLFileName") or die "Couldn't open file $IDLFileName";

    # Add header
    print OUTPUTIDL @IDLHeader;

    # Add forward declarations and imorts
    delete $IDLForwardDeclarations{keys(%IDLDontForwardDeclare)};
    delete $IDLImports{keys(%IDLDontImport)};

    print OUTPUTIDL map { "cpp_quote(\"interface $_;\")\n" } sort keys(%IDLForwardDeclarations);
    print OUTPUTIDL "\n";

    print OUTPUTIDL map { "interface $_;\n" } sort keys(%IDLForwardDeclarations);
    print OUTPUTIDL "\n";
    print OUTPUTIDL "#ifndef DO_NO_IMPORTS\n";
    print OUTPUTIDL map { ($_ eq "IGEN_DOMImplementation") ? "import \"IGEN_DOMDOMImplementation.idl\";\n" : "import \"$_.idl\";\n" } sort keys(%IDLImports);
    print OUTPUTIDL "#endif\n";
    print OUTPUTIDL "\n";

    # Add content
    print OUTPUTIDL @IDLContent;

    close(OUTPUTIDL);

    @IDLHeader = ();
    @IDLContent = ();

    if ($pureInterface) {
        my $CPPInterfaceHeaderFileName = "$outputDir/" . $TEMP_PREFIX . "DOM" . $name . ".h";
        unlink($CPPInterfaceHeaderFileName);

        open(OUTPUTCPPInterfaceHeader, ">$CPPInterfaceHeaderFileName") or die "Couldn't open file $CPPInterfaceHeaderFileName";

        print OUTPUTCPPInterfaceHeader @CPPInterfaceHeader;

        close(OUTPUTCPPInterfaceHeader);

        @CPPInterfaceHeader = ();
    } else {
        my $CPPHeaderFileName = "$outputDir/" . $TEMP_PREFIX . "DOM" . $name . ".h";
        unlink($CPPHeaderFileName);

        # -- CPP Header --
        open(OUTPUTCPPHeader, ">$CPPHeaderFileName") or die "Couldn't open file $CPPHeaderFileName";

        # Add header
        print OUTPUTCPPHeader @CPPHeaderHeader;

        # Add includes
        print OUTPUTCPPHeader map { ($_ eq "GEN_DOMImplementation.h") ? "#include \"GEN_DOMDOMImplementation.h\"\n" : "#include \"$_\"\n" } sort keys(%CPPHeaderIncludes);
        print OUTPUTCPPHeader map { "#include <$_>\n" } sort keys(%CPPHeaderIncludesAngle);

        foreach my $dontDeclare (keys(%CPPHeaderDontForwardDeclarations)) {
            delete $CPPHeaderForwardDeclarations{$dontDeclare} if ($CPPHeaderForwardDeclarations{$dontDeclare});
        }
        print OUTPUTCPPHeader "\n";
        print OUTPUTCPPHeader map { "interface $_;\n" } sort keys(%CPPHeaderForwardDeclarations);
        print OUTPUTCPPHeader "\n";

        # Add content
        print OUTPUTCPPHeader @CPPHeaderContent;

        close(OUTPUTCPPHeader);

        @CPPHeaderHeader = ();
        @CPPHeaderContent = ();


        # -- CPP Implementation --
        my $CPPImplementationFileName = "$outputDir/" . $TEMP_PREFIX . "DOM" . $name . ".cpp";
        unlink($CPPImplementationFileName);

        open(OUTPUTCPPImplementation, ">$CPPImplementationFileName") or die "Couldn't open file $CPPImplementationFileName";

        # Add header
        print OUTPUTCPPImplementation @CPPImplementationHeader;
        print OUTPUTCPPImplementation "\n";

        # Add includes
        foreach my $dontInclude (keys(%CPPImplementationDontIncludes)) {
            delete $CPPImplementationIncludes{$dontInclude} if ($CPPImplementationIncludes{$dontInclude});
        }
        print OUTPUTCPPImplementation map { ($_ eq "GEN_DOMImplementation.h") ? "#include \"GEN_DOMDOMImplementation.h\"\n" : "#include \"$_\"\n" } sort keys(%CPPImplementationIncludes);
        print OUTPUTCPPImplementation map { "#include <$_>\n" } sort keys(%CPPImplementationIncludesAngle);
        print OUTPUTCPPImplementation "\n";

        print OUTPUTCPPImplementation "#pragma warning(push, 0)\n";
        print OUTPUTCPPImplementation map { "#include <WebCore/$_>\n" } sort keys(%CPPImplementationWebCoreIncludes);
        print OUTPUTCPPImplementation "#pragma warning(pop)\n";

        # Add content
        print OUTPUTCPPImplementation @CPPImplementationContent;

        close(OUTPUTCPPImplementation);

        @CPPImplementationHeader = ();
        @CPPImplementationContent = ();
    }
}

1;
