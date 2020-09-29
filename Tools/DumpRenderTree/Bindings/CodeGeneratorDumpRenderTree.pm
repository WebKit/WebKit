# Copyright (C) 2016-2020 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

use strict;
use warnings;
use File::Spec;

package CodeGeneratorDumpRenderTree;

sub new
{
    my ($class, $codeGenerator, $writeDependencies, $verbose, $idlFilePath) = @_;

    my $reference = {
        codeGenerator => $codeGenerator,
        idlFilePath => $idlFilePath,
    };

    bless($reference, $class);
    return $reference;
}

sub GenerateInterface
{
}

sub WriteData
{
    my ($self, $interface, $outputDir) = @_;

    foreach my $file ($self->_generateHeaderFile($interface), $self->_generateImplementationFile($interface)) {
        $$self{codeGenerator}->UpdateFile(File::Spec->catfile($outputDir, $$file{name}), join("", @{$$file{contents}}));
    }
}

sub _className
{
    my ($type) = @_;

    return "JS" . _implementationClassName($type);
}

sub _classRefGetter
{
    my ($self, $type) = @_;

    return $$self{codeGenerator}->WK_lcfirst(_implementationClassName($type)) . "Class";
}

sub _parseLicenseBlock
{
    my ($fileHandle) = @_;

    my ($copyright, $readCount, $buffer, $currentCharacter, $previousCharacter);
    my $startSentinel = "/*";
    my $lengthOfStartSentinel = length($startSentinel);
    $readCount = read($fileHandle, $buffer, $lengthOfStartSentinel);
    return "" if ($readCount < $lengthOfStartSentinel || $buffer ne $startSentinel);
    $copyright = $buffer;

    while ($readCount = read($fileHandle, $currentCharacter, 1)) {
        $copyright .= $currentCharacter;
        return $copyright if $currentCharacter eq "/" && $previousCharacter eq "*";
        $previousCharacter = $currentCharacter;
    }

    return "";
}

sub _parseLicenseBlockFromFile
{
    my ($path) = @_;
    open my $fileHandle, "<", $path or die "Failed to open $path for reading: $!";
    my $licenseBlock = _parseLicenseBlock($fileHandle);
    close($fileHandle);
    return $licenseBlock;
}

sub _defaultLicenseBlock
{
    return <<EOF;
/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
EOF
}

sub _licenseBlock
{
    my ($self) = @_;
    return $self->{licenseBlock} if $self->{licenseBlock};

    my $licenseBlock = _parseLicenseBlockFromFile($self->{idlFilePath}) || _defaultLicenseBlock();
    $self->{licenseBlock} = $licenseBlock;
    return $licenseBlock;
}

sub _generateHeaderFile
{
    my ($self, $interface) = @_;

    my @contents = ();

    my $type = $interface->type;
    my $className = _className($type);
    my $implementationClassName = _implementationClassName($type);
    my $filename = $className . ".h";

    push(@contents, $self->_licenseBlock());

    my $parentClassName = _parentClassName($interface);

    push(@contents, <<EOF);

#ifndef ${className}_h
#define ${className}_h

#include "${parentClassName}.h"
EOF
    push(@contents, <<EOF);

namespace WTR {

class ${implementationClassName};

class ${className} : public ${parentClassName} {
public:
    static JSClassRef @{[$self->_classRefGetter($type)]}();

private:
    static const JSStaticFunction* staticFunctions();
    static const JSStaticValue* staticValues();
EOF

    if (my @operations = @{$interface->operations}) {
        push(@contents, "\n    // Functions\n\n");
        foreach my $operation (@operations) {
            push(@contents, "    static JSValueRef @{[$operation->name]}(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);\n");
        }
    }

    if (my @attributes = @{$interface->attributes}) {
        push(@contents, "\n    // Attributes\n\n");
        foreach my $attribute (@attributes) {
            push(@contents, "    static JSValueRef @{[$self->_getterName($attribute)]}(JSContextRef, JSObjectRef, JSStringRef, JSValueRef*);\n");
            push(@contents, "    static bool @{[$self->_setterName($attribute)]}(JSContextRef, JSObjectRef, JSStringRef, JSValueRef, JSValueRef*);\n") unless $attribute->isReadOnly;
        }
    }

    push(@contents, <<EOF);
};
    
${implementationClassName}* to${implementationClassName}(JSContextRef, JSValueRef);

} // namespace WTR

#endif // ${className}_h
EOF

    return { name => $filename, contents => \@contents };
}

sub _generateImplementationFile
{
    my ($self, $interface) = @_;

    my @contentsPrefix = ();
    my %contentsIncludes = ();
    my @contents = ();

    my $type = $interface->type;
    my $className = _className($type);
    my $implementationClassName = _implementationClassName($type);
    my $filename = $className . ".cpp";

    push(@contentsPrefix, $self->_licenseBlock());

    my $classRefGetter = $self->_classRefGetter($type);
    my $parentClassName = _parentClassName($interface);

    $contentsIncludes{"${className}.h"} = 1;
    $contentsIncludes{"${implementationClassName}.h"} = 1;

    push(@contentsPrefix, <<EOF);

EOF

    push(@contents, <<EOF);
#include <JavaScriptCore/JSRetainPtr.h>
#include <wtf/GetPtr.h>

namespace WTR {

${implementationClassName}* to${implementationClassName}(JSContextRef context, JSValueRef value)
{
    if (!context || !value || !${className}::${classRefGetter}() || !JSValueIsObjectOfClass(context, value, ${className}::${classRefGetter}()))
        return 0;
    return static_cast<${implementationClassName}*>(JSWrapper::unwrap(context, value));
}

JSClassRef ${className}::${classRefGetter}()
{
    static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "@{[$type->name]}";
        definition.parentClass = @{[$self->_parentClassRefGetterExpression($interface)]};
        definition.staticValues = staticValues();
        definition.staticFunctions = staticFunctions();
EOF

    push(@contents, "        definition.initialize = initialize;\n") unless _parentInterface($interface);
    push(@contents, "        definition.finalize = finalize;\n") unless _parentInterface($interface);

    push(@contents, <<EOF);
        jsClass = JSClassCreate(&definition);
    }
    return jsClass;
}

EOF

    push(@contents, $self->_staticFunctionsGetterImplementation($interface), "\n");
    push(@contents, $self->_staticValuesGetterImplementation($interface));

    if (my @operations = @{$interface->operations}) {
        push(@contents, "\n// Functions\n");

        foreach my $operation (@operations) {
            push(@contents, <<EOF);

JSValueRef ${className}::@{[$operation->name]}(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    ${implementationClassName}* impl = to${implementationClassName}(context, thisObject);
    if (!impl)
        return JSValueMakeUndefined(context);

EOF
            my $functionCall;
            if ($operation->extendedAttributes->{"CustomArgumentHandling"}) {
                $functionCall = "impl->" . $operation->name . "(context, argumentCount, arguments, exception)";
            } else {
                my @arguments = ();
                my @specifiedArguments = @{$operation->arguments};

                $self->_includeHeaders(\%contentsIncludes, $operation->type);

                if ($operation->extendedAttributes->{"PassContext"}) {
                    push(@arguments, "context");
                }

                foreach my $i (0..$#specifiedArguments) {
                    my $argument = $specifiedArguments[$i];

                    $self->_includeHeaders(\%contentsIncludes, $type);

                    push(@contents, "    " . $self->_platformTypeVariableDeclaration($argument->type, $argument->name, "arguments[$i]", "argumentCount > $i") . "\n");
                    
                    push(@arguments, $self->_argumentExpression($argument));
                }

                $functionCall = "impl->" . $operation->name . "(" . join(", ", @arguments) . ")";
            }
            
            push(@contents, "    ${functionCall};\n\n") if $operation->type->name eq "undefined";
            push(@contents, "    return " . $self->_returnExpression($operation->type, $functionCall) . ";\n}\n");
        }
    }

    if (my @attributes = @{$interface->attributes}) {
        push(@contents, "\n// Attributes\n");
        foreach my $attribute (@attributes) {
            $self->_includeHeaders(\%contentsIncludes, $attribute->type);

            my $getterName = $self->_getterName($attribute);
            my $getterExpression = "impl->${getterName}()";

            push(@contents, <<EOF);

JSValueRef ${className}::${getterName}(JSContextRef context, JSObjectRef object, JSStringRef, JSValueRef* exception)
{
    ${implementationClassName}* impl = to${implementationClassName}(context, object);
    if (!impl)
        return JSValueMakeUndefined(context);

    return @{[$self->_returnExpression($attribute->type, $getterExpression)]};
}
EOF

            unless ($attribute->isReadOnly) {
                push(@contents, <<EOF);

bool ${className}::@{[$self->_setterName($attribute)]}(JSContextRef context, JSObjectRef object, JSStringRef, JSValueRef value, JSValueRef* exception)
{
    ${implementationClassName}* impl = to${implementationClassName}(context, object);
    if (!impl)
        return false;

EOF

                my $platformValue = $self->_platformTypeConstructor($attribute->type, "value");

                push(@contents, <<EOF);
    impl->@{[$self->_setterName($attribute)]}(${platformValue});

    return true;
}
EOF
            }
        }
    }

    push(@contents, <<EOF);

} // namespace WTR

EOF

    unshift(@contents, map { "#include \"$_\"\n" } sort keys(%contentsIncludes));
    unshift(@contents, "#include \"config.h\"\n");
    unshift(@contents, @contentsPrefix);

    return { name => $filename, contents => \@contents };
}

sub _getterName
{
    my ($self, $attribute) = @_;

    return $attribute->name;
}

sub _includeHeaders
{
    my ($self, $headers, $type) = @_;

    return unless defined $type;
    return if $type->name eq "boolean";
    return if $type->name eq "object";
    return if $$self{codeGenerator}->IsPrimitiveType($type);
    return if $$self{codeGenerator}->IsStringType($type);

    $$headers{_className($type) . ".h"} = 1;
    $$headers{_implementationClassName($type) . ".h"} = 1;
}

sub _implementationClassName
{
    my ($type) = @_;

    return $type->name;
}

sub _parentClassName
{
    my ($interface) = @_;

    my $parentInterface = _parentInterface($interface);
    return $parentInterface ? _className($parentInterface) : "JSWrapper";
}

sub _parentClassRefGetterExpression
{
    my ($self, $interface) = @_;

    my $parentInterface = _parentInterface($interface);
    return $parentInterface ? $self->_classRefGetter($parentInterface) . "()" : "0";
}

sub _parentInterface
{
    my ($interface) = @_;
    return $interface->parentType;
}

sub _platformType
{
    my ($self, $type) = @_;

    return undef unless defined $type;

    return "bool" if $type->name eq "boolean";
    return "JSValueRef" if $type->name eq "object";
    return "JSRetainPtr<JSStringRef>" if $$self{codeGenerator}->IsStringType($type);
    return "double" if $$self{codeGenerator}->IsPrimitiveType($type);
    return _implementationClassName($type);
}

sub _platformTypeConstructor
{
    my ($self, $type, $argumentName) = @_;

    return "toOptionalBool(context, $argumentName)" if $type->name eq "boolean" && $type->isNullable;
    return "JSValueToBoolean(context, $argumentName)" if $type->name eq "boolean";
    return "$argumentName" if $type->name eq "object";
    return "createJSString(context, $argumentName)" if $$self{codeGenerator}->IsStringType($type);
    return "JSValueToNumber(context, $argumentName, nullptr)" if $$self{codeGenerator}->IsPrimitiveType($type);
    return "to" . _implementationClassName($type) . "(context, $argumentName)";
}

sub _platformTypeVariableDeclaration
{
    my ($self, $type, $variableName, $argumentName, $condition) = @_;

    my $platformType = $self->_platformType($type);
    my $constructor = $self->_platformTypeConstructor($type, $argumentName);

    my %nonPointerTypes = (
        "bool" => 1,
        "double" => 1,
        "JSRetainPtr<JSStringRef>" => 1,
        "JSValueRef" => 1,
    );

    my $nullValue = "0";
    if ($platformType eq "JSValueRef") {
        $nullValue = "JSValueMakeUndefined(context)";
    } elsif (defined $nonPointerTypes{$platformType} && $platformType ne "double") {
        $nullValue = "$platformType()";
    }

    $platformType .= "*" unless defined $nonPointerTypes{$platformType};

    return "$platformType $variableName = $condition && $constructor;" if $condition && $platformType eq "bool";
    return "$platformType $variableName = $condition ? $constructor : $nullValue;" if $condition;
    return "$platformType $variableName = $constructor;";
}

sub _returnExpression
{
    my ($self, $returnType, $expression) = @_;

    return "JSValueMakeUndefined(context)" if $returnType->name eq "undefined";
    return "makeValue(context, ${expression})" if $returnType->name eq "boolean" && $returnType->isNullable;
    return "JSValueMakeBoolean(context, ${expression})" if $returnType->name eq "boolean";
    return "${expression}" if $returnType->name eq "object";
    return "JSValueMakeNumber(context, ${expression})" if $$self{codeGenerator}->IsPrimitiveType($returnType);
    return "makeValue(context, ${expression}.get())" if $$self{codeGenerator}->IsStringType($returnType);
    return "toJS(context, WTF::getPtr(${expression}))";
}

sub _argumentExpression
{
    my ($self, $argument) = @_;

    my $type = $argument->type;
    my $name = $argument->name;

    return "${name}.get()" if $$self{codeGenerator}->IsStringType($type);
    return $name;
}

sub _setterName
{
    my ($self, $attribute) = @_;

    my $name = $attribute->name;

    return "set" . $$self{codeGenerator}->WK_ucfirst($name);
}

sub _staticFunctionsGetterImplementation
{
    my ($self, $interface) = @_;

    my $mapFunction = sub {
        my $name = $_->name;
        my @attributes = qw(kJSPropertyAttributeDontDelete kJSPropertyAttributeReadOnly);
        push(@attributes, "kJSPropertyAttributeDontEnum") if $_->extendedAttributes->{"DontEnum"};

        return  "{ \"$name\", $name, " . join(" | ", @attributes) . " }";
    };

    return $self->_staticFunctionsOrValuesGetterImplementation($interface, "function", "{ 0, 0, 0 }", $mapFunction, $interface->operations);
}

sub _staticFunctionsOrValuesGetterImplementation
{
    my ($self, $interface, $functionOrValue, $arrayTerminator, $mapFunction, $operationsOrAttributes) = @_;

    my $className = _className($interface->type);
    my $uppercaseFunctionOrValue = $$self{codeGenerator}->WK_ucfirst($functionOrValue);

    my $result = <<EOF;
const JSStatic${uppercaseFunctionOrValue}* ${className}::static${uppercaseFunctionOrValue}s()
{
EOF

    my @initializers = map(&$mapFunction, @{$operationsOrAttributes});
    return $result . "    return 0;\n}\n" unless @initializers;

    $result .= <<EOF
    static const JSStatic${uppercaseFunctionOrValue} ${functionOrValue}s[] = {
        @{[join(",\n        ", @initializers)]},
        ${arrayTerminator}
    };
    return ${functionOrValue}s;
}
EOF
}

sub _staticValuesGetterImplementation
{
    my ($self, $interface) = @_;

    my $mapFunction = sub {
        return if $_->extendedAttributes->{"NoImplementation"};

        my $attributeName = $_->name;
        my $getterName = $self->_getterName($_);
        my $setterName = $_->isReadOnly ? "0" : $self->_setterName($_);
        my @attributes = qw(kJSPropertyAttributeDontDelete);
        push(@attributes, "kJSPropertyAttributeReadOnly") if $_->isReadOnly;
        push(@attributes, "kJSPropertyAttributeDontEnum") if $_->extendedAttributes->{"DontEnum"};

        return "{ \"$attributeName\", $getterName, $setterName, " . join(" | ", @attributes) . " }";
    };

    return $self->_staticFunctionsOrValuesGetterImplementation($interface, "value", "{ 0, 0, 0, 0 }", $mapFunction, $interface->attributes);
}

1;
