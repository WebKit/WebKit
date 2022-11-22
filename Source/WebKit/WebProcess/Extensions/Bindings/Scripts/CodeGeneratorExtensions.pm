# Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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

package CodeGeneratorExtensions;

use strict;
use warnings;
use File::Spec;

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
    my ($idlType) = @_;

    return "JS" . _implementationClassName($idlType);
}

sub _classRefGetter
{
    my ($self, $idlType, $suffix) = @_;

    $suffix = "Class" unless defined $suffix;

    my $implementationClassName = _implementationClassName($idlType);
    $implementationClassName =~ s/^WebExtensionAPI//;

    return $$self{codeGenerator}->WK_lcfirst($implementationClassName) . $suffix;
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
 * Copyright (C) 2020-@{[_yearString()]} Apple Inc. All rights reserved.
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

sub conditionalString
{
    my ($node) = @_;
    my $conditional = $node->extendedAttributes->{"Conditional"};
    return "" unless $conditional;
    # We really should be calling this as $$self{codeGenerator}->Generate..., but that makes it hard
    # to call this function from ExtensionsGlobalObjectsCollector.pl. Since
    # GenerateConditionalStringFromAttributeValue doesn't actually use the object reference, this
    # way works just fine.
    return CodeGenerator::GenerateConditionalStringFromAttributeValue(0, $conditional);
}

sub _generateHeaderFile
{
    my ($self, $interface) = @_;

    my @contents = ();

    push(@contents, $self->_licenseBlock());
    push(@contents, "\n\n");

    push(@contents, "#pragma once\n\n");

    my $idlType = $interface->type;
    my $className = _className($idlType);
    my $implementationClassName = _implementationClassName($idlType);
    my $parentClassName = $self->_parentClassName($interface);
    my $filename = $className . ".h";

    my $conditionalString = conditionalString($interface);
    push(@contents, <<EOF) if $conditionalString;
#if ${conditionalString}

EOF

    push(@contents, <<EOF);
#include "${parentClassName}.h"
EOF

    push(@contents, "#include \"${className}Custom.h\"\n") if $interface->extendedAttributes->{"CustomHeader"};
    push(@contents, <<EOF);

namespace WebKit {

class ${implementationClassName};

class ${className} : public ${parentClassName} {
public:
    static JSClassRef @{[$self->_classRefGetter($idlType)]}();
    static JSClassRef @{[$self->_classRefGetter($idlType, "GlobalObjectClass")]}();

private:
    static const JSStaticFunction* staticFunctions();
    static const JSStaticValue* staticValues();
    static const JSStaticValue* staticConstants();
    static bool hasInstance(JSContextRef, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef* exception);
EOF

    my $hasDynamicProperties = 0;
    my $hasMainWorldOnlyProperties = 0;

    if (my @functions = @{$interface->operations}) {
        push(@contents, "\n    // Functions\n\n");
        push(@contents, "    static void getPropertyNames(JSContextRef, JSObjectRef, JSPropertyNameAccumulatorRef);\n") if $interface->extendedAttributes->{"CustomGetPropertyNames"};
        push(@contents, "    static bool hasProperty(JSContextRef, JSObjectRef, JSStringRef);\n") if $interface->extendedAttributes->{"CustomHasProperty"};

        foreach my $function (@functions) {
            $hasDynamicProperties = 1 if $function->extendedAttributes->{"Dynamic"};
            $hasMainWorldOnlyProperties = 1 if $function->extendedAttributes->{"MainWorldOnly"};

            my $conditionalString = conditionalString($function);

            push(@contents, "#if ${conditionalString}\n") if $conditionalString;
            push(@contents, "    static JSValueRef @{[$function->name]}(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);\n");
            push(@contents, "    static bool setProperty(JSContextRef, JSObjectRef, JSStringRef, JSValueRef, JSValueRef*);\n") if $function->extendedAttributes->{"SetProperty"};
            push(@contents, "    static JSValueRef getProperty(JSContextRef, JSObjectRef, JSStringRef, JSValueRef*);\n") if $function->extendedAttributes->{"GetProperty"};
            push(@contents, "    static bool deleteProperty(JSContextRef, JSObjectRef, JSStringRef, JSValueRef*);\n") if $function->extendedAttributes->{"DeleteProperty"};
            push(@contents, "#endif // ${conditionalString}\n") if $conditionalString;
        }
    }

    if (my @constants = @{$interface->constants}) {
        push(@contents, "\n    // Constants\n\n");
        foreach my $constant (@constants) {
            my $getterName = $constant->name;
            push(@contents, "    static JSValueRef ${getterName}(JSContextRef, JSObjectRef, JSStringRef, JSValueRef*);\n");
        }
    }

    if (my @attributes = @{$interface->attributes}) {
        push(@contents, "\n    // Attributes\n\n");
        foreach my $attribute (@attributes) {
            next if $attribute->extendedAttributes->{"NoImplementation"};

            $hasDynamicProperties = 1 if $attribute->extendedAttributes->{"Dynamic"};
            $hasMainWorldOnlyProperties = 1 if $attribute->extendedAttributes->{"MainWorldOnly"};

            my $conditionalString = conditionalString($attribute);

            push(@contents, "#if ${conditionalString}\n") if $conditionalString;
            push(@contents, "    static JSValueRef @{[$self->_getterName($attribute)]}(JSContextRef, JSObjectRef, JSStringRef, JSValueRef*);\n");
            push(@contents, "    static bool @{[$self->_setterName($attribute)]}(JSContextRef, JSObjectRef, JSStringRef, JSValueRef, JSValueRef*);\n") unless $attribute->isReadOnly;
            push(@contents, "#endif // ${conditionalString}\n") if $conditionalString;
        }
    }

    if ($hasDynamicProperties or $hasMainWorldOnlyProperties) {
        push(@contents, "\n    // Dynamic Attributes\n\n");
        push(@contents, "    static void getPropertyNames(JSContextRef, JSObjectRef, JSPropertyNameAccumulatorRef);\n");
        push(@contents, "    static bool hasProperty(JSContextRef, JSObjectRef, JSStringRef);\n");
        push(@contents, "    static JSValueRef getProperty(JSContextRef, JSObjectRef, JSStringRef, JSValueRef*);\n");
    }

    push(@contents, <<EOF);
};

${implementationClassName}* to${implementationClassName}(JSContextRef, JSValueRef);

} // namespace WebKit

EOF

    push(@contents, <<EOF) if $conditionalString;
#endif // ${conditionalString}

EOF

    push(@contents, <<EOF);
EOF

    return { name => $filename, contents => \@contents };
}

sub _generateImplementationFile
{
    my ($self, $interface) = @_;

    my @contentsPrefix = ();
    my %contentsIncludes = ();
    my @contents = ();

    my $idlType = $interface->type;
    my $className = _className($idlType);
    my $implementationClassName = _implementationClassName($idlType);
    my $filename = $className . ".mm";

    push(@contentsPrefix, $self->_licenseBlock());
    push(@contentsPrefix, "\n\n");

    push(@contentsPrefix, <<EOF);
#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

EOF

    push(@contentsPrefix, "#include \"config.h\"\n\n");

    my $conditionalString = conditionalString($interface);
    push(@contentsPrefix, <<EOF) if $conditionalString;
#if ${conditionalString}

EOF

    my $classRefGetter = $self->_classRefGetter($idlType);
    my $globalClassRefGetter = $self->_classRefGetter($idlType, "GlobalObjectClass");
    my $parentClassName = $self->_parentClassName($interface);

    $contentsIncludes{"\"${className}.h\""} = 1;
    $contentsIncludes{"\"${implementationClassName}.h\""} = 1;

    push(@contents, <<EOF);
#include <wtf/GetPtr.h>

namespace WebKit {

${implementationClassName}* to${implementationClassName}(JSContextRef context, JSValueRef value)
{
    if (!context || !value || !${className}::${classRefGetter}() || !JSValueIsObjectOfClass(context, value, ${className}::${classRefGetter}()))
        return nullptr;
    return static_cast<${implementationClassName}*>(JSWebExtensionWrapper::unwrap(context, value));
}

JSClassRef ${className}::${classRefGetter}()
{
    static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "@{[_publicClassName($idlType)]}";
        definition.parentClass = @{[$self->_parentClassRefGetterExpression($interface)]};
        definition.staticValues = staticValues();
        definition.staticFunctions = staticFunctions();
EOF

    push(@contents, "        definition.initialize = initialize;\n") unless _parentInterface($interface);
    push(@contents, "        definition.finalize = finalize;\n") unless _parentInterface($interface);
    push(@contents, "        definition.getPropertyNames = getPropertyNames;\n") if $interface->extendedAttributes->{"CustomGetPropertyNames"};
    push(@contents, "        definition.hasProperty = hasProperty;\n") if $interface->extendedAttributes->{"CustomHasProperty"};

    my $hasDynamicProperties = 0;
    my $hasMainWorldOnlyProperties = 0;

    if (my @functions = @{$interface->operations}) {
        foreach my $function (@functions) {
            $hasDynamicProperties = 1 if $function->extendedAttributes->{"Dynamic"};
            $hasMainWorldOnlyProperties = 1 if $function->extendedAttributes->{"MainWorldOnly"};

            push(@contents, "        definition.setProperty = setProperty;\n") if $function->extendedAttributes->{"SetProperty"};
            push(@contents, "        definition.getProperty = getProperty;\n") if $function->extendedAttributes->{"GetProperty"};
            push(@contents, "        definition.deleteProperty = deleteProperty;\n") if $function->extendedAttributes->{"DeleteProperty"};
        }
    }

    if (my @attributes = @{$interface->attributes}) {
        foreach my $attribute (@attributes) {
            $hasDynamicProperties = 1 if $attribute->extendedAttributes->{"Dynamic"};
            $hasMainWorldOnlyProperties = 1 if $attribute->extendedAttributes->{"MainWorldOnly"};
        }
    }

    if ($hasDynamicProperties or $hasMainWorldOnlyProperties) {
        push(@contents, "        definition.getPropertyNames = getPropertyNames;\n");
        push(@contents, "        definition.hasProperty = hasProperty;\n");
        push(@contents, "        definition.getProperty = getProperty;\n");
    }

    push(@contents, <<EOF);
        jsClass = JSClassCreate(&definition);
    }

    return jsClass;
}

EOF
    push (@contents, <<EOF);
JSClassRef ${className}::${globalClassRefGetter}()
{
    static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "@{[_publicClassName($idlType)]}";
        definition.staticValues = staticConstants();
        definition.hasInstance = hasInstance;
        jsClass = JSClassCreate(&definition);
    }

    return jsClass;
}

EOF

    push(@contents, $self->_staticFunctionsGetterImplementation($interface), "\n");
    push(@contents, $self->_staticValuesGetterImplementation($interface), "\n");
    push(@contents, $self->_staticConstantsGetterImplementation($interface), "\n");
    push(@contents, $self->_hasInstanceImplementation($interface));
    push(@contents, $self->_dynamicAttributesImplementation($interface, $hasDynamicProperties, $hasMainWorldOnlyProperties));

    my $earlyReturnCondition = "!impl";
    $earlyReturnCondition .= " || !impl->isForMainWorld()" if $interface->extendedAttributes->{"MainWorldOnly"};

    if (my @functions = @{$interface->operations}) {
        push(@contents, "\n// Functions\n");

        if (my $customGetPropertyNamesFunction = $interface->extendedAttributes->{"CustomGetPropertyNames"}) {
            push(@contents, <<EOF);

void ${className}::getPropertyNames(JSContextRef context, JSObjectRef thisObject, JSPropertyNameAccumulatorRef propertyNames)
{
    ${implementationClassName}* impl = to${implementationClassName}(context, thisObject);
    if (!impl)
        return;

    NSArray *propertyNameStrings = impl->${customGetPropertyNamesFunction}();

    for (NSString *propertyName in propertyNameStrings)
        JSPropertyNameAccumulatorAddName(propertyNames, toJSString(propertyName).get());
}
EOF
        }

        if (my $customHasPropertyFunction = $interface->extendedAttributes->{"CustomHasProperty"}) {
            push(@contents, <<EOF);

bool ${className}::hasProperty(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName)
{
    ${implementationClassName}* impl = to${implementationClassName}(context, thisObject);
    if (!impl)
        return false;

    return impl->${customHasPropertyFunction}(toNSString(propertyName));
}
EOF
        }

        foreach my $function (@functions) {
            next if $function->extendedAttributes->{"Custom"};

            my $functionEarlyReturnCondition = $earlyReturnCondition;

            push(@contents, "\n");

            my $functionSignature = "JSValueRef ${className}::@{[$function->name]}(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)";

            my $defaultEarlyReturnValue = "JSValueMakeUndefined(context)";
            my $defaultReturnValue = $defaultEarlyReturnValue;

            my $isSetPropertyFunction = defined $function->extendedAttributes->{"SetProperty"};
            my $isGetPropertyFunction = defined $function->extendedAttributes->{"GetProperty"};
            my $isDeletePropertyFunction = defined $function->extendedAttributes->{"DeleteProperty"};
            my $isPropertyFunction = $isSetPropertyFunction || $isGetPropertyFunction || $isDeletePropertyFunction;

            my $conditionalString = conditionalString($function);
            push(@contents, <<EOF) if $conditionalString;
#if ${conditionalString}
EOF

            if ($isSetPropertyFunction) {
                $defaultEarlyReturnValue = "false";
                $defaultReturnValue = "true";
                push(@contents, "bool ${className}::setProperty(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef newValue, JSValueRef* exception)");
            } elsif ($isGetPropertyFunction) {
                push(@contents, "JSValueRef ${className}::getProperty(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)");
            } elsif ($isDeletePropertyFunction) {
                $defaultEarlyReturnValue = "false";
                $defaultReturnValue = "true";
                push(@contents, "bool ${className}::deleteProperty(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)");
            } else {
                push(@contents, $functionSignature);
            }

            push(@contents, <<EOF);

{
    ${implementationClassName}* impl = to${implementationClassName}(context, thisObject);
    if (${functionEarlyReturnCondition})
        return ${defaultEarlyReturnValue};
EOF

            my @parameters = ();
            my @specifiedParameters = @{$function->arguments};
            my $needsExceptionString = $function->extendedAttributes->{"RaisesException"};

            my $lastParameter = $specifiedParameters[$#specifiedParameters];

            push(@contents, "\n") if scalar @specifiedParameters;
            my $needsScriptContext = $function->extendedAttributes->{"NeedsScriptContext"};

            my $needsFrame = $function->extendedAttributes->{"NeedsFrame"};
            my $needsPage = $function->extendedAttributes->{"NeedsPage"};
            my $returnsPromiseIfNoCallback = $function->extendedAttributes->{"ReturnsPromiseWhenCallbackIsOmitted"} || $interface->extendedAttributes->{"ReturnsPromiseWhenCallbackIsOmitted"};
            my $callbackHandlerArgument;

            $self->_includeHeaders(\%contentsIncludes, $function->type, $function);

            my $optionalArgumentCount = 0;

            foreach my $parameter (@specifiedParameters) {
                $self->_includeHeaders(\%contentsIncludes, $parameter->type, $parameter);
                $optionalArgumentCount++ if $parameter->extendedAttributes->{"Optional"};
                $needsPage = 1 if $parameter->extendedAttributes->{"CallbackHandler"} && $interface->extendedAttributes->{"NeedsPageWithCallbackHandler"};
                $needsFrame = 1 if $parameter->extendedAttributes->{"CallbackHandler"} && $interface->extendedAttributes->{"NeedsFrameWithCallbackHandler"};
                $callbackHandlerArgument = $parameter->name if $parameter->extendedAttributes->{"CallbackHandler"} && $parameter->extendedAttributes->{"Optional"};
            }

            my $requiredArgumentCount = scalar @specifiedParameters - $optionalArgumentCount;

            if ($requiredArgumentCount) {
                my $call = _callString($idlType, $function, 1);
                push(@contents, <<EOF);
    const size_t requiredArgumentCount = ${requiredArgumentCount};
    if (argumentCount < requiredArgumentCount) {
        NSString *requiredArgumentsMissingExceptionString = @"Invalid call to ${call}. A required argument is missing.";
        *exception = toJSError(context, requiredArgumentsMissingExceptionString);
        return ${defaultEarlyReturnValue};
    }

EOF
            }

            if ($optionalArgumentCount) {
                die "Property functions can't take optional arguments" if $isPropertyFunction;

                foreach my $parameter (@specifiedParameters) {
                    push(@contents, "    " . $self->_platformTypeVariableDeclaration($parameter, $parameter->name) . "\n");
                    push(@parameters, $parameter->name . ".releaseNonNull()") if $parameter->extendedAttributes->{"CallbackHandler"} && $parameter->extendedAttributes->{"Optional"};
                    push(@parameters, $parameter->name) unless $parameter->extendedAttributes->{"CallbackHandler"} && $parameter->extendedAttributes->{"Optional"};
                }

                push(@contents, "\n");

                push(@contents, "    if (argumentCount == " . scalar @specifiedParameters . ") {\n");

                my $hasOptionalCallbackHandlerAsLastArgument = 1 if $lastParameter->extendedAttributes->{"CallbackHandler"} && $lastParameter->extendedAttributes->{"Optional"};

                foreach my $i (0..$#specifiedParameters) {
                    my $parameter = $specifiedParameters[$i];
                    push(@contents, "        " . $self->_platformTypeVariableDeclaration($parameter, $parameter->name, "arguments[${i}]", undef, 1) . "\n");
                }

                push(@contents, "    } else {\n");

                my $processArgumentsLeftToRight = $function->extendedAttributes->{"ProcessArgumentsLeftToRight"};

                push(@contents, "        ssize_t argumentIndex = argumentCount - 1;\n") unless $processArgumentsLeftToRight;
                push(@contents, "        size_t argumentIndex = 0;\n") if $processArgumentsLeftToRight;
                push(@contents, "        JSValueRef currentArgument = nullptr;\n\n");

                push(@contents, "        size_t allowedOptionalArgumentCount = argumentCount - requiredArgumentCount;\n") if $requiredArgumentCount;
                push(@contents, "        size_t processedOptionalArgumentCount = 0;\n\n") if $requiredArgumentCount;

                if ($hasOptionalCallbackHandlerAsLastArgument && $processArgumentsLeftToRight) {
                    my $parameter = $specifiedParameters[$#specifiedParameters];
                    my $optionalCondition = $requiredArgumentCount ? "allowedOptionalArgumentCount && " : "";
                    push(@contents, "        if ($optionalCondition(currentArgument = arguments[argumentCount - 1]) && (" . $self->_javaScriptTypeCondition($parameter, "currentArgument") . ")) {\n");
                    push(@contents, "            " . $self->_platformTypeVariableDeclaration($parameter, $parameter->name, "currentArgument", undef, 1) . "\n");
                    push(@contents, "            --allowedOptionalArgumentCount;\n") if $requiredArgumentCount;
                    push(@contents, "            if (argumentCount)\n");
                    push(@contents, "                --argumentCount;\n");
                    push(@contents, "        }\n\n");
                }

                my $argumentIndexConditon = $processArgumentsLeftToRight ? "argumentIndex < argumentCount" : "argumentIndex >= 0";
                my $nextArgumentIndex = $processArgumentsLeftToRight ? "++argumentIndex" : "--argumentIndex";

                for (my $j = $processArgumentsLeftToRight ? 0 : $#specifiedParameters; $processArgumentsLeftToRight ? $j <= $#specifiedParameters : $j >= 0; $processArgumentsLeftToRight ? $j++ : $j--) {
                    my $parameter = $specifiedParameters[$j];
                    my $lastArgument = $j eq ($processArgumentsLeftToRight ? $#specifiedParameters : 0);

                    next if $lastArgument && $hasOptionalCallbackHandlerAsLastArgument && $processArgumentsLeftToRight;

                    my $optionalCondition = $requiredArgumentCount && $parameter->extendedAttributes->{"Optional"} ? " && processedOptionalArgumentCount < allowedOptionalArgumentCount" : "";
                    push(@contents, "        if ($argumentIndexConditon$optionalCondition && (currentArgument = arguments[argumentIndex])) {\n");

                    if ($parameter->extendedAttributes->{"Optional"}) {
                        push(@contents, "            if (" . $self->_javaScriptTypeCondition($parameter, "currentArgument") . ") {\n");
                        push(@contents, "                " . $self->_platformTypeVariableDeclaration($parameter, $parameter->name, "currentArgument", undef, 1) . "\n");
                        push(@contents, "                ++processedOptionalArgumentCount;\n") if $requiredArgumentCount;
                        push(@contents, "                $nextArgumentIndex;\n");
                        push(@contents, "            }\n");
                    } else {
                        push(@contents, "            " . $self->_platformTypeVariableDeclaration($parameter, $parameter->name, "currentArgument", undef, 1) . "\n");
                        push(@contents, "            $nextArgumentIndex;\n");
                    }

                    push(@contents, "        }\n");

                    push(@contents, "\n") unless $lastArgument || ($j eq $#specifiedParameters - 1 && $hasOptionalCallbackHandlerAsLastArgument && $processArgumentsLeftToRight);
                }

                push(@contents, "    }\n");
            } else {
                foreach my $i (0..$#specifiedParameters) {
                    my $parameter = $specifiedParameters[$i];

                    if ($isPropertyFunction && $i == 0) {
                        push(@contents, "    NSString *" . $parameter->name . " = toNSString(propertyName);\n");
                    } else {
                        my $argument = $isSetPropertyFunction ? "newValue" : "arguments[$i]";

                        die "GetProperty and DeleteProperty functions only take one argument" if $isGetPropertyFunction || $isDeletePropertyFunction;

                        push(@contents, "    " . $self->_platformTypeVariableDeclaration($parameter, $parameter->name, $argument) . "\n");
                    }

                    push(@parameters, $parameter->name . ".releaseNonNull()") if $parameter->extendedAttributes->{"CallbackHandler"} && $parameter->extendedAttributes->{"Optional"};
                    push(@parameters, $parameter->name) unless $parameter->extendedAttributes->{"CallbackHandler"} && $parameter->extendedAttributes->{"Optional"};
                }
            }

            my $mapFunction = sub {
                return $_->extendedAttributes->{"ImplementedAs"} if $_->extendedAttributes->{"ImplementedAs"};
                return $_->name;
            };

            my @methodSignatureNames = map(&$mapFunction, @specifiedParameters);

            foreach my $parameter (@specifiedParameters) {
                $self->_installAutomaticExceptions(\@contents, $parameter, $idlType, $parameter->name, $parameter->name, $defaultReturnValue, \%contentsIncludes, $function, 1);
            }

            unshift(@methodSignatureNames, "context") if $needsScriptContext;
            unshift(@parameters, "context") if $needsScriptContext;

            unshift(@methodSignatureNames, "page") if $needsPage;
            unshift(@parameters, "toWebPage(context)") if $needsPage;

            unshift(@methodSignatureNames, "frame") if $needsFrame;
            unshift(@parameters, "toWebFrame(context)") if $needsFrame;

            push(@methodSignatureNames, "outExceptionString") if $needsExceptionString;
            push(@parameters, "&exceptionString") if $needsExceptionString;

            my $isVoidReturn = $function->type->name eq "void";
            my $functionCall = $self->_functionCall($function, \@methodSignatureNames, \@parameters, $interface);
            my $returnExpression = $self->_returnExpression($function, $functionCall, $interface);

            push(@contents, "\n");

            if ($callbackHandlerArgument && $returnsPromiseIfNoCallback) {
                die "Returning a Promise is only allowed for void functions" unless $isVoidReturn;

                $defaultReturnValue = "promiseResult ?: $defaultReturnValue";

                push(@contents, <<EOF);
    JSObjectRef promiseResult = nullptr;
    if (!${callbackHandlerArgument}) {
        JSObjectRef resolveFunction, rejectFunction = nullptr;
        promiseResult = JSObjectMakeDeferredPromise(context, &resolveFunction, &rejectFunction, nullptr);
        ${callbackHandlerArgument} = toJSPromiseCallbackHandler(context, resolveFunction, rejectFunction);
    }

EOF
            }

            if ($callbackHandlerArgument) {
                push(@contents, <<EOF);
    if (!${callbackHandlerArgument})
        ${callbackHandlerArgument} = toJSErrorCallbackHandler(context, impl->runtime());

EOF
            }

            if ($needsExceptionString && !$isVoidReturn) {
                push(@contents, <<EOF);
    NSString *exceptionString = nil;
    JSValueRef result = ${returnExpression};

    if (exceptionString) {
        *exception = toJSError(context, exceptionString);
        return ${defaultReturnValue};
    }

    return result;
}
EOF
            } elsif ($needsExceptionString && $isVoidReturn) {
                push(@contents, <<EOF);
    NSString *exceptionString = nil;
    ${functionCall};

    if (exceptionString)
        *exception = toJSError(context, exceptionString);

    return ${defaultReturnValue};
}
EOF
            } elsif ($isVoidReturn) {
                push(@contents, "    ${functionCall};\n\n");
                push(@contents, "    return ${defaultReturnValue};\n}\n");
            } else {
                push(@contents, "    return " . $returnExpression . ";\n}\n");
            }

            if ($isPropertyFunction) {
                push(@contents, <<EOF);

${functionSignature}
{
    ${implementationClassName}* impl = to${implementationClassName}(context, thisObject);
    if (${functionEarlyReturnCondition})
        return JSValueMakeUndefined(context);

EOF

                my $keyArgumentName = $specifiedParameters[0]->name;
                push(@contents, "    auto ${keyArgumentName} = toJSString(argumentCount > 0 ? " . $self->_platformTypeConstructor($specifiedParameters[0], "arguments[0]") . " : nil);\n");

                if ($isGetPropertyFunction) {
                    push(@contents, "\n");
                    push(@contents, "    return getProperty(context, thisObject, ${keyArgumentName}.get(), exception);\n");
                } elsif ($isSetPropertyFunction) {
                    my $valueArgumentName = $specifiedParameters[1]->name;
                    push(@contents, "    JSValueRef ${valueArgumentName} = argumentCount > 1 ? arguments[1] : JSValueMakeUndefined(context);\n\n");
                    push(@contents, "    setProperty(context, thisObject, ${keyArgumentName}.get(), ${valueArgumentName}, exception);\n\n");
                } elsif ($isDeletePropertyFunction) {
                    push(@contents, "\n");
                    push(@contents, "    deleteProperty(context, thisObject, ${keyArgumentName}.get(), exception);\n\n");
                } else {
                    die "Unknown property function";
                }

                push(@contents, "    return JSValueMakeUndefined(context);\n") if $isSetPropertyFunction || $isDeletePropertyFunction;

                push(@contents, "}\n");
            }

            push(@contents, <<EOF) if $conditionalString;
#endif // ${conditionalString}
EOF
        }
    }

    if (my @constants = @{$interface->constants}) {
        push(@contents, "\n// Constants\n");
        foreach my $constant (@constants) {
            my $getterName = $constant->name;
            my $constantValue = $constant->value;
            push(@contents, <<EOF);

JSValueRef ${className}::${getterName}(JSContextRef context, JSObjectRef object, JSStringRef, JSValueRef* exception)
{
    return JSValueMakeNumber(context, $constantValue);
}
EOF
        }
    }

    if (my @attributes = @{$interface->attributes}) {
        push(@contents, "\n// Attributes\n");
        foreach my $attribute (@attributes) {
            next if $attribute->extendedAttributes->{"NoImplementation"} or $attribute->extendedAttributes->{"Custom"};

            $self->_includeHeaders(\%contentsIncludes, $attribute->type, $attribute);

            my $getterName = $self->_getterName($attribute);

            my @methodSignatureNames = ();
            my @parameters = ();

            push(@methodSignatureNames, "context") if $attribute->extendedAttributes->{"NeedsScriptContext"};
            push(@parameters, "context") if $attribute->extendedAttributes->{"NeedsScriptContext"};

            push(@methodSignatureNames, "page") if $attribute->extendedAttributes->{"NeedsPage"};
            push(@parameters, "toWebPage(context)") if $attribute->extendedAttributes->{"NeedsPage"};

            push(@methodSignatureNames, "frame") if $attribute->extendedAttributes->{"NeedsFrame"};
            push(@parameters, "toWebFrame(context)") if $attribute->extendedAttributes->{"NeedsFrame"};

            my $getterExpression = $self->_functionCall($attribute, \@methodSignatureNames, \@parameters, $interface, $getterName);

            my $getterEarlyReturnCondition = $earlyReturnCondition;

            push(@contents, <<EOF);

EOF

            my $conditionalString = conditionalString($attribute);
            push(@contents, <<EOF) if $conditionalString;
#if ${conditionalString}
EOF

            push(@contents, <<EOF);
JSValueRef ${className}::${getterName}(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
EOF

            push(@contents, "    UNUSED_PARAM(propertyName);\n\n");

            push(@contents, <<EOF);
    ${implementationClassName}* impl = to${implementationClassName}(context, object);
    if (${getterEarlyReturnCondition})
        return JSValueMakeUndefined(context);
EOF

            push(@contents, <<EOF);

    return @{[$self->_returnExpression($attribute, $getterExpression, $interface)]};
}
EOF

            unless ($attribute->isReadOnly) {
                my $setterEarlyReturnCondition = $earlyReturnCondition;

                push(@contents, <<EOF);

bool ${className}::@{[$self->_setterName($attribute)]}(JSContextRef context, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
EOF

                push(@contents, "    UNUSED_PARAM(propertyName);\n\n");

                push(@contents, <<EOF);
    ${implementationClassName}* impl = to${implementationClassName}(context, object);
    if (${setterEarlyReturnCondition})
        return false;

EOF

                my $platformValue;
                if ($self->_hasAutomaticExceptions($attribute)) {
                    $platformValue = "platformValue";
                    push(@contents, "    " . $self->_platformTypeVariableDeclaration($attribute, "platformValue", "value") . "\n");
                    $self->_installAutomaticExceptions(\@contents, $attribute, $idlType, "platformValue", $attribute->name, "false", \%contentsIncludes, $attribute);
                } else {
                    $platformValue = $self->_platformTypeConstructor($attribute, "value");
                }

                push(@contents, <<EOF);
    impl->@{[$self->_setterName($attribute)]}(${platformValue});

    return true;
}
EOF
            }

            push(@contents, <<EOF) if $conditionalString;
#endif // ${conditionalString}
EOF
        }
    }

    push(@contents, <<EOF);

} // namespace WebKit
EOF

    push(@contents, <<EOF) if $conditionalString;

#endif // ${conditionalString}
EOF

    unshift(@contents, map { "#include $_\n" } sort keys(%contentsIncludes));
    unshift(@contents, @contentsPrefix);

    return { name => $filename, contents => \@contents };
}

sub _getterName
{
    my ($self, $attribute) = @_;

    my $signature = $attribute;
    my $name = $signature->name;

    my %specialCases = (
        bubbles => "bubbles",
    );

    return $specialCases{$name} if defined $specialCases{$name};
    return "is" . $$self{codeGenerator}->WK_ucfirst($name) if $signature->type->name eq "boolean";
    return $signature->extendedAttributes->{"ImplementedAs"} if $signature->extendedAttributes->{"ImplementedAs"};
    return $name;
}

sub _hasAutomaticExceptions
{
    my ($self, $signature) = @_;

    return $signature->extendedAttributes->{"CannotBeEmpty"} || $signature->extendedAttributes->{"Serialization"} eq "JSON" || $signature->extendedAttributes->{"NSArray"}
        || $signature->extendedAttributes->{"NSDictionary"} || $signature->extendedAttributes->{"NSObject"} || $signature->extendedAttributes->{"URL"}
        || $signature->type->name eq "function" || $$self{codeGenerator}->IsPrimitiveType($signature->type) || $$self{codeGenerator}->IsStringType($signature->type);
}

sub _includeHeaders
{
    my ($self, $headers, $idlType, $signature) = @_;

    return unless defined $idlType;

    my $idlTypeName = $idlType->name;
    return if $idlTypeName eq "DOMString" or $idlTypeName eq "DOMWindow" or $idlTypeName eq "any" or $idlTypeName eq "void" or $idlTypeName eq "boolean" or $idlTypeName eq "function" or $idlTypeName eq "array" or $$self{codeGenerator}->IsPrimitiveType($idlType);

    $$headers{'"' . _className($idlType) . '.h"'} = 1;
    $$headers{'"' . _implementationClassName($idlType) . '.h"'} = 1;
}

sub _implementationClassName
{
    my ($idlType) = @_;
    return $idlType->name;
}

sub _callString
{
    my ($classIDLType, $functionOrAttribute, $isFunction) = @_;

    my $className = CodeGenerator::WK_lcfirst(undef, _publicClassName($classIDLType));
    my $name = $functionOrAttribute->name;

    # Some cases make more sense to not include the parent name,
    # those are represented with empty strings, and it excludes
    # the call path in the output.
    my %specialCases = (
        action => "browserAction",
        event => "",
        localization => "i18n",
        namespace => "",
        webNavigationEvent => "",
        webRequestEvent => "",
        windowEvent => "",
    );

    $className = $specialCases{$className} if defined $specialCases{$className};

    my $call = "${className}.${name}";
    $call = $name unless $className;
    $call .= "()" if $isFunction;

    return $call;
}

sub _installAutomaticExceptions
{
    my ($self, $contents, $signature, $classIDLType, $variable, $variableLabel, $result, $contentsIncludes, $functionOrAttribute, $isFunction) = @_;

    my $call = _callString($classIDLType, $functionOrAttribute, $isFunction);

    my $hasExceptions = 0;

    if ($signature->extendedAttributes->{"Serialization"} && $signature->extendedAttributes->{"Serialization"} eq "JSON") {
        $hasExceptions = 1;

        push(@$contents, <<EOF);

    if (*exception)
        return ${result};
EOF
    }

    if ($$self{codeGenerator}->IsStringType($signature->type) && !$signature->extendedAttributes->{"Optional"}) {
        $hasExceptions = 1;

        push(@$contents, <<EOF);

    if (!$variable) {
        NSString *mustBeAStringString = @"Invalid '${variableLabel}' value passed to ${call}. Expected a string.";
        *exception = toJSError(context, mustBeAStringString);
        return ${result};
    }
EOF
    }

    if ($$self{codeGenerator}->IsPrimitiveType($signature->type) && $signature->type->name ne "boolean" && !$signature->extendedAttributes->{"Optional"}) {
        $hasExceptions = 1;

        push(@$contents, <<EOF);

    if (isnan($variable)) {
        NSString *mustBeANumberString = @"Invalid '${variableLabel}' value passed to ${call}. Expected a number.";
        *exception = toJSError(context, mustBeANumberString);
        return ${result};
    }
EOF
    }

    if ($signature->type->name eq "any" && ($signature->extendedAttributes->{"NSDictionary"} || $signature->extendedAttributes->{"NSObject"} || $signature->extendedAttributes->{"Serialization"}) && !$signature->extendedAttributes->{"Optional"}) {
        $hasExceptions = 1;

        push(@$contents, <<EOF);

    if (!$variable) {
        NSString *mustBeAnObjectString = @"Invalid '${variableLabel}' value passed to ${call}. Expected an object.";
        *exception = toJSError(context, mustBeAnObjectString);
        return ${result};
    }
EOF
    }

    if ($signature->type->name eq "any" && !($signature->extendedAttributes->{"NSDictionary"} || $signature->extendedAttributes->{"NSObject"} || $signature->extendedAttributes->{"Serialization"}) && !$signature->extendedAttributes->{"Optional"} && !$signature->extendedAttributes->{"ValuesAllowed"}) {
        $hasExceptions = 1;

        push(@$contents, <<EOF);

    if ($variable && !$variable.isObject) {
        NSString *mustBeAnObjectString = @"Invalid '${variableLabel}' value passed to ${call}. Expected an object.";
        *exception = toJSError(context, mustBeAnObjectString);
        return ${result};
    }
EOF
    }

    if ($signature->type->name eq "any" && $signature->extendedAttributes->{"NSArray"} && !$signature->extendedAttributes->{"Optional"}) {
        $hasExceptions = 1;

        push(@$contents, <<EOF);

    if (!$variable) {
        NSString *mustBeAnArrayString = @"Invalid '${variableLabel}' value passed to ${call}. Expected an array.";
        *exception = toJSError(context, mustBeAnArrayString);
        return ${result};
    }
EOF
    }

    if ($signature->extendedAttributes->{"CannotBeEmpty"}) {
        $hasExceptions = 1;

        my $isEmptyCheck;
        if ($signature->extendedAttributes->{"URL"}) {
            $isEmptyCheck = "!${variable}.relativeString.length";
        } else {
            $isEmptyCheck = "!${variable}.length";
        }

        push(@$contents, <<EOF);

    if (${isEmptyCheck}) {
        NSString *cannotBeEmptyExceptionString = @"Invalid '${variableLabel}' value passed to ${call}. Cannot be empty.";
        *exception = toJSError(context, cannotBeEmptyExceptionString);
        return ${result};
    }
EOF
    }

    if ($signature->extendedAttributes->{"URL"}) {
        $hasExceptions = 1;

        # FIXME: rdar://problem/58428135 Consider allowing local file access if the extension claimed it in
        # its manifest and the user opted in explicity in some way.

        push(@$contents, <<EOF);

    if (${variable}.isFileURL) {
        NSString *cannotBeAccessedExceptionString = @"Invalid '${variableLabel}' value passed to ${call}. Cannot be a local file URL.";
        *exception = toJSError(context, cannotBeAccessedExceptionString);
        return ${result};
    }
EOF
    }

    if ($signature->type->name eq "function" && !$signature->extendedAttributes->{"CallbackHandler"} && !$signature->extendedAttributes->{"Optional"}) {
        $hasExceptions = 1;

        push(@$contents, <<EOF);

    if ($variable && !JSObjectIsFunction(context, $variable)) {
        NSString *mustBeAFunctionString = @"Invalid '${variableLabel}' value passed to ${call}. Expected a function.";
        *exception = toJSError(context, mustBeAFunctionString);
        return ${result};
    }
EOF
    }

    if ($signature->type->name eq "function" && $signature->extendedAttributes->{"CallbackHandler"} && !$signature->extendedAttributes->{"Optional"}) {
        $hasExceptions = 1;

        push(@$contents, <<EOF);

    if (!$variable) {
        NSString *mustBeAFunctionString = @"Invalid '${variableLabel}' value passed to ${call}. Expected a function.";
        *exception = toJSError(context, mustBeAFunctionString);
        return ${result};
    }
EOF
    }

    return $hasExceptions;
}

sub _yearString
{
    my (undef, undef, undef, undef, undef, $year) = localtime(time);

    return $year;
}

sub _nullStringPolicy
{
    my ($signature) = @_;

    die "Both NullAsNullString and ConvertNullAndUndefinedToString were specified" if $signature->extendedAttributes->{"ConvertNullToNullString"} && $signature->extendedAttributes->{"ConvertNullAndUndefinedToString"};

    return "NullStringPolicy::NullAsNullString" if $signature->extendedAttributes->{"ConvertNullToNullString"};
    return "NullStringPolicy::NoNullString" if $signature->extendedAttributes->{"ConvertNullAndUndefinedToString"};
    return "NullStringPolicy::NullAndUndefinedAsNullString";
}

sub _publicClassName
{
    my ($idlType) = @_;
    my $idlTypeName = $idlType->name;
    $idlTypeName =~ s/^WebExtensionAPI//;
    return $idlTypeName;
}

sub _parentClassName
{
    my ($self, $interface) = @_;

    my $parentInterface = _parentInterface($interface);
    return $parentInterface ? _className($parentInterface) : "JSWebExtensionWrapper";
}

sub _parentClassRefGetterExpression
{
    my ($self, $interface) = @_;

    my $parentInterface = _parentInterface($interface);
    return $parentInterface ? $self->_classRefGetter($parentInterface) . "()" : "nullptr";
}

sub _parentInterface
{
    my ($interface) = @_;
    return $interface->parentType;
}

sub _javaScriptTypeCondition
{
    my ($self, $signature, $argument) = @_;

    my $idlType = $signature->type;

    return undef unless defined $idlType;

    my $idlTypeName = $idlType->name;
    my $nullOrUndefined = "JSValueIsNull(context, ${argument}) || JSValueIsUndefined(context, ${argument})";

    return "(JSValueIsObject(context, ${argument}) && !JSObjectIsFunction(context, JSValueToObject(context, ${argument}, nullptr))) || JSValueIsString(context, ${argument}) || ${nullOrUndefined}" if $idlTypeName eq "any" && $signature->extendedAttributes->{"NSObject"} && $signature->extendedAttributes->{"DOMString"};
    return "(JSValueIsObject(context, ${argument}) && !JSObjectIsFunction(context, JSValueToObject(context, ${argument}, nullptr))) || ${nullOrUndefined}" if $idlTypeName eq "any" && ($signature->extendedAttributes->{"NSObject"} || $signature->extendedAttributes->{"NSArray"} || $signature->extendedAttributes->{"NSDictionary"} || $signature->extendedAttributes->{"Serialization"});
    return "JSValueIsObject(context, ${argument}) || ${nullOrUndefined}" if $idlTypeName eq "any" && !$signature->extendedAttributes->{"ValuesAllowed"};
    return "(JSValueIsObject(context, ${argument}) && JSObjectIsFunction(context, JSValueToObject(context, ${argument}, nullptr))) || ${nullOrUndefined}" if $idlTypeName eq "function";
    return "JSValueIsBoolean(context, ${argument}) || ${nullOrUndefined}" if $idlTypeName eq "boolean";
    return "JSValueIsArray(context, ${argument}) || ${nullOrUndefined}" if $idlTypeName eq "array";
    return "JSValueIsString(context, ${argument}) || ${nullOrUndefined}" if $$self{codeGenerator}->IsStringType($idlType);
    return "JSValueIsNumber(context, ${argument}) || ${nullOrUndefined}"  if $$self{codeGenerator}->IsPrimitiveType($idlType);
    return $argument;
}

sub _platformType
{
    my ($self, $idlType, $signature) = @_;

    return undef unless defined $idlType;

    my $idlTypeName = $idlType;
    $idlTypeName = $idlType->name if ref($idlType) eq "IDLType";

    return "RefPtr<WebExtensionCallbackHandler>" if $idlTypeName eq "function" && $signature->extendedAttributes->{"CallbackHandler"};
    return "NSArray" if $idlTypeName eq "array" && $signature && $signature->extendedAttributes->{"NSArray"};
    return "NSString" if $idlTypeName eq "any" && $signature->extendedAttributes->{"Serialization"};
    return "NSDictionary" if $idlTypeName eq "any" && $signature && $signature->extendedAttributes->{"NSDictionary"};
    return "NSObject" if $idlTypeName eq "any" && $signature && $signature->extendedAttributes->{"NSObject"};
    return "JSValue" if $idlTypeName eq "DOMWindow" || $idlTypeName eq "function" || $idlTypeName eq "any";
    return "bool" if $idlTypeName eq "boolean";

    return unless ref($idlType) eq "IDLType";

    return "NSURL" if $$self{codeGenerator}->IsStringType($idlType) && $signature && $signature->extendedAttributes->{"URL"};
    return "NSString" if $$self{codeGenerator}->IsStringType($idlType);
    return "double" if $$self{codeGenerator}->IsPrimitiveType($idlType);

    return _implementationClassName($idlType);
}

sub _platformTypeConstructor
{
    my ($self, $signature, $argumentName) = @_;

    my $idlType = $signature->type;

    my $idlTypeName = $idlType;
    $idlTypeName = $idlType->name if ref($idlType) eq "IDLType";

    my $nullStringPolicy = _nullStringPolicy($signature);
    my $arrayType = $signature->extendedAttributes->{"NSArray"};

    if ($idlTypeName eq "any") {
        return "serializeJSObject(context, $argumentName, exception)" if $signature->extendedAttributes->{"Serialization"} && $signature->extendedAttributes->{"Serialization"} eq "JSON";
        return "toNSDictionary(context, $argumentName)" if $signature->extendedAttributes->{"NSDictionary"};
        return "dynamic_objc_cast<NSArray>(toNSObject(context, $argumentName))" if $signature->extendedAttributes->{"NSArray"};
        return "toNSObject(context, $argumentName)" if $signature->extendedAttributes->{"NSObject"};
        return "toJSValue(context, $argumentName)";
    }

    return "toJSCallbackHandler(context, $argumentName, impl->runtime())" if $idlTypeName eq "function" && $signature->extendedAttributes->{"CallbackHandler"};
    return "dynamic_objc_cast<NSArray>(toNSObject(context, $argumentName, $arrayType.class))" if $idlTypeName eq "array" && $arrayType;
    return "JSValueToBoolean(context, $argumentName)" if $idlTypeName eq "boolean";
    return "toJSValue(context, $argumentName)" if $idlTypeName eq "function";

    return unless ref($idlType) eq "IDLType";

    return "[NSURL URLWithString:toNSString(context, $argumentName, $nullStringPolicy)]" if $$self{codeGenerator}->IsStringType($idlType) && $signature->extendedAttributes->{"URL"};
    return "toNSString(context, $argumentName, $nullStringPolicy)" if $$self{codeGenerator}->IsStringType($idlType);
    return "JSValueToNumber(context, $argumentName, nullptr)" if $$self{codeGenerator}->IsPrimitiveType($idlType);
    return "to" . _implementationClassName($idlType) . "(context, $argumentName)";
}

sub _platformTypeVariableDeclaration
{
    my ($self, $signature, $variableName, $argumentName, $condition, $hideType) = @_;

    my $idlType = $signature->type;
    my $idlTypeName = $idlType->name;
    my $platformType = $self->_platformType($idlType, $signature);
    my $constructor = $self->_platformTypeConstructor($signature, $argumentName) if $argumentName;

    my %objCTypes = (
        "JSValue"       => 1,
        "NSArray"       => 1,
        "NSDictionary"  => 1,
        "NSURL"         => 1,
        "NSString"      => 1,
        "NSObject"      => 1,
    );

    my $nullValue = "nullptr";
    $nullValue = "false" if $platformType eq "bool";
    $nullValue = "std::numeric_limits<double>::quiet_NaN()" if $platformType eq "double";
    $nullValue = "nil" if $objCTypes{$platformType};
    $nullValue = "JSValueMakeUndefined(context)" if $platformType eq "JSValueRef";

    my $defaultValue = $signature->extendedAttributes->{"DefaultValue"};
    if (defined $defaultValue && $platformType eq "double") {
        $nullValue = $defaultValue eq "Max" ? "std::numeric_limits<double>::max()" : $defaultValue;
    } elsif (defined $defaultValue) {
        die "DefaultValue extended attribute is currently only supported for numeric types";
    }

    $condition = "JSValueIsNumber(context, $argumentName)" if $platformType eq "double" and $argumentName and !$condition;

    if ($platformType eq "JSValueRef" or $platformType eq "JSObjectRef" or $platformType eq "RefPtr<WebExtensionCallbackHandler>" or $platformType eq "double" or $platformType eq "bool") {
        $platformType .= " ";
    } else {
        $platformType .= $objCTypes{$platformType} ? " *" : "* ";
    }

    $platformType = "" if $hideType;

    return "$platformType$variableName = $condition && $constructor;" if $condition && $platformType eq "bool ";
    return "$platformType$variableName = $condition ? $constructor : $nullValue;" if $condition;
    return "$platformType$variableName;" if $platformType =~ /^RefPtr</;
    return "$platformType$variableName = $constructor;" if $constructor;
    return "$platformType$variableName = $nullValue;";
}

sub _functionCall
{
    my ($self, $signature, $methodSignatureNamesRef, $parametersRef, $interface, $getterName) = @_;

    my @methodSignatureNames = @$methodSignatureNamesRef;
    my @parameters = @$parametersRef;

    my $functionName = "impl->" . $signature->name;
    $functionName = "impl->" . $getterName if $getterName;
    $functionName = "impl->" . $signature->extendedAttributes->{"ImplementedAs"} if $signature->extendedAttributes->{"ImplementedAs"};
    return $functionName . "(" . join(", ", @parameters) . ")";
}

sub _returnExpression
{
    my ($self, $signature, $expression, $interface) = @_;

    my $convertNullStringAttribute = $signature->extendedAttributes->{"ConvertNullStringTo"};
    my $nullOrEmptyString = "NullOrEmptyString::NullStringAsEmptyString";
    $nullOrEmptyString = "NullOrEmptyString::NullStringAsNull" if defined $convertNullStringAttribute && $convertNullStringAttribute eq "Null";

    my $returnIDLType = $signature->type;
    my $returnIDLTypeName = $returnIDLType->name;

    return "deserializeJSONString(context, $expression)" if $returnIDLTypeName eq "any" && $signature->extendedAttributes->{"Serialization"} && $signature->extendedAttributes->{"Serialization"} eq "JSON";
    return "toJSValueRefOrJSNull(context, $expression)" if $returnIDLTypeName eq "any" || $returnIDLTypeName eq "DOMWindow";
    return "toJSValueRef(context, ${expression}, $nullOrEmptyString)" if $$self{codeGenerator}->IsStringType($returnIDLType);
    return "JSValueMakeUndefined(context)" if $returnIDLTypeName eq "void";
    return "JSValueMakeBoolean(context, ${expression})" if $returnIDLTypeName eq "boolean";
    return "JSValueMakeNumber(context, ${expression})" if $$self{codeGenerator}->IsPrimitiveType($returnIDLType);
    return "toJS(context, WTF::getPtr(${expression}))";
}

sub _setterName
{
    my ($self, $attribute) = @_;

    my $name = $attribute->name;

    my %specialCases = (
        url => "URL"
    );

    return "set" . $specialCases{$name} if defined $specialCases{$name};
    return "set" . $$self{codeGenerator}->WK_ucfirst($name);
}

sub _staticFunctionsGetterImplementation
{
    my ($self, $interface) = @_;

    my $mapFunction = sub {
        return if $_->extendedAttributes->{"Dynamic"} or $_->extendedAttributes->{"MainWorldOnly"};

        my $name = $_->name;
        my @attributes = ();
        push(@attributes, "kJSPropertyAttributeDontEnum") if $_->extendedAttributes->{"DontEnum"};
        my $jsproperties = scalar @attributes == 0 ? "kJSPropertyAttributeNone" : join(" | ", @attributes);
        my $conditionalString = conditionalString($_);

        return "#if ${conditionalString}\n        { \"$name\", $name, $jsproperties },\n        #endif" if $conditionalString;
        return "{ \"$name\", $name, $jsproperties },";
    };

    my @initializers = map(&$mapFunction, @{$interface->operations});
    return $self->_staticFunctionsOrValuesOrConstantsGetterImplementation($interface, "function", "{ nullptr, nullptr, kJSPropertyAttributeNone },", @initializers);
}

sub _staticFunctionsOrValuesOrConstantsGetterImplementation
{
    my ($self, $interface, $functionOrValueOrConstant, $arrayTerminator, @initializers) = @_;

    my $className = _className($interface->type);
    my $uppercaseFunctionOrValueOrConstant = $$self{codeGenerator}->WK_ucfirst($functionOrValueOrConstant);
    my $functionOrValueType = $uppercaseFunctionOrValueOrConstant;
    $functionOrValueType = "Value" if $functionOrValueType eq "Constant";

    my $result = <<EOF;
const JSStatic${functionOrValueType}* ${className}::static${uppercaseFunctionOrValueOrConstant}s()
{
EOF

    return $result . "    return nullptr;\n}\n" unless @initializers;

    $result .= <<EOF
    static const JSStatic${functionOrValueType} ${functionOrValueOrConstant}s[] = {
        @{[join("\n        ", @initializers)]}
        ${arrayTerminator}
    };

    return ${functionOrValueOrConstant}s;
}
EOF
}

sub _staticValuesGetterImplementation
{
    my ($self, $interface) = @_;

    my $mapFunction = sub {
        return if $_->extendedAttributes->{"NoImplementation"} or $_->extendedAttributes->{"Dynamic"} or $_->extendedAttributes->{"MainWorldOnly"};

        my $attributeName = $_->name;
        my $attributeIsReadonly = $_->isReadOnly;
        my $getterName = $self->_getterName($_);
        my $setterName = $attributeIsReadonly ? "nullptr" : $self->_setterName($_);
        my @attributes = ();
        push(@attributes, "kJSPropertyAttributeDontEnum") if $_->extendedAttributes->{"DontEnum"};
        my $jsproperties = scalar @attributes == 0 ? "kJSPropertyAttributeNone" : join(" | ", @attributes);
        my $conditionalString = conditionalString($_);

        return "#if ${conditionalString}\n        { \"$attributeName\", $getterName, $setterName, $jsproperties },\n        #endif" if $conditionalString;
        return "{ \"$attributeName\", $getterName, $setterName, $jsproperties },";
    };

    my @initializersWithConstants = (map(&$mapFunction, @{$interface->attributes}), $self->_staticConstants($interface));

    return $self->_staticFunctionsOrValuesOrConstantsGetterImplementation($interface, "value", "{ nullptr, nullptr, nullptr, kJSPropertyAttributeNone },", @initializersWithConstants);
}

sub _staticConstants
{
    my ($self, $interface) = @_;
    my @initializers = ();
    foreach my $constant (@{$interface->constants}) {
        my $constantName = $constant->name;
        push(@initializers, "{ \"$constantName\", $constantName, nullptr, kJSPropertyAttributeNone }");
    }
    return @initializers;
}

sub _staticConstantsGetterImplementation
{
    my ($self, $interface) = @_;
    return $self->_staticFunctionsOrValuesOrConstantsGetterImplementation($interface, "constant", "{ nullptr, nullptr, nullptr, kJSPropertyAttributeNone },", $self->_staticConstants($interface));
}

sub _hasInstanceImplementation
{
    my ($self, $interface) = @_;

    my $className = _className($interface->type);
    my $classRefGetter = $self->_classRefGetter($interface->type);

    my $result = <<EOF;
bool ${className}::hasInstance(JSContextRef ctx, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef* exception)
{
    return JSValueIsObjectOfClass(ctx, possibleInstance, ${classRefGetter}());
}
EOF
}

sub _dynamicAttributesImplementation
{
    my ($self, $interface, $hasDynamicProperties, $hasMainWorldOnlyProperties) = @_;

    return unless $hasDynamicProperties or $hasMainWorldOnlyProperties;

    my $idlType = $interface->type;
    my $idlTypeName = $idlType->name;
    my $className = _className($idlType);
    my $implementationClassName = _implementationClassName($idlType);

    my @contents = ();

    push(@contents, <<EOF);

void ${className}::getPropertyNames(JSContextRef context, JSObjectRef thisObject, JSPropertyNameAccumulatorRef propertyNames)
{
    ${implementationClassName}* impl = to${implementationClassName}(context, thisObject);
    if (!impl)
        return;

EOF
    my $generateCondition = sub {
        my ($interface, $middleCondition) = @_;
        my $name = $interface->name;
        my @conditions = ();
        push(@conditions, "isForMainWorld") if $interface->extendedAttributes->{"MainWorldOnly"};
        push(@conditions, $middleCondition) if $middleCondition;
        push(@conditions, "impl->isPropertyAllowed(\"${name}\"_s, toWebPage(context))") if $interface->extendedAttributes->{"Dynamic"};
        return join(" && ", @conditions);
    };

    push(@contents, "    bool isForMainWorld = impl->isForMainWorld();\n") if $hasMainWorldOnlyProperties;
    push(@contents, "\n") if $hasMainWorldOnlyProperties and $hasDynamicProperties;

    if ($hasMainWorldOnlyProperties and !$hasDynamicProperties) {
        push(@contents, "    if (!isForMainWorld)\n");
        push(@contents, "        return;\n\n");
    }

    my $mapFunction = sub {
        return unless $_->extendedAttributes->{"Dynamic"} or $_->extendedAttributes->{"MainWorldOnly"};

        my $name = $_->name;
        return "    JSPropertyNameAccumulatorAddName(propertyNames, toJSString(\"${name}\").get());\n" unless $hasDynamicProperties;

        my $condition = &$generateCondition($_);
        my $conditionalString = conditionalString($_);

        my $content = "";
        $content .= "#if ${conditionalString}\n" if $conditionalString;
        $content .= "    if (${condition})\n";
        $content .= "        JSPropertyNameAccumulatorAddName(propertyNames, toJSString(\"${name}\").get());\n";
        $content .= "#endif // ${conditionalString}\n" if $conditionalString;
        $content .= "\n";
        return $content;
    };

    push(@contents, map(&$mapFunction, @{$interface->operations})) if @{$interface->operations};
    push(@contents, map(&$mapFunction, @{$interface->attributes})) if @{$interface->attributes};

    push(@contents, <<EOF);
}

bool ${className}::hasProperty(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName)
{
    ${implementationClassName}* impl = to${implementationClassName}(context, thisObject);
    if (!impl)
        return false;

EOF

    push(@contents, "    bool isForMainWorld = impl->isForMainWorld();\n\n") if $hasMainWorldOnlyProperties;

    $mapFunction = sub {
        return unless $_->extendedAttributes->{"Dynamic"} or $_->extendedAttributes->{"MainWorldOnly"};

        my $name = $_->name;
        my $condition = &$generateCondition($_);
        my $conditionalString = conditionalString($_);

        my $content = "";
        $content .= "#if ${conditionalString}\n" if $conditionalString;
        $content .= "    if (JSStringIsEqualToUTF8CString(propertyName, \"${name}\"))\n";
        $content .= "        return ${condition};\n";
        $content .= "#endif // ${conditionalString}\n" if $conditionalString;
        $content .= "\n";
        return $content;
    };

    push(@contents, map(&$mapFunction, @{$interface->operations})) if @{$interface->operations};
    push(@contents, map(&$mapFunction, @{$interface->attributes})) if @{$interface->attributes};

    push(@contents, "    return false;\n");

    push(@contents, <<EOF);
}

JSValueRef ${className}::getProperty(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    ${implementationClassName}* impl = to${implementationClassName}(context, thisObject);
    if (!impl)
        return JSValueMakeUndefined(context);

EOF

    push(@contents, "    bool isForMainWorld = impl->isForMainWorld();\n") if $hasMainWorldOnlyProperties;
    push(@contents, "\n") if $hasMainWorldOnlyProperties and $hasDynamicProperties;

    if ($hasMainWorldOnlyProperties and !$hasDynamicProperties) {
        push(@contents, "    if (!isForMainWorld)\n");
        push(@contents, "        return JSValueMakeUndefined(context);\n\n");
    }

    if (my @attributes = @{$interface->attributes}) {
        foreach my $attribute (@attributes) {
            next unless $attribute->extendedAttributes->{"Dynamic"} or $attribute->extendedAttributes->{"MainWorldOnly"};

            my $name = $attribute->name;
            my $getterName = $self->_getterName($attribute);

            my $condition = "JSStringIsEqualToUTF8CString(propertyName, \"${name}\")" if $hasMainWorldOnlyProperties and !$hasDynamicProperties;
            $condition = &$generateCondition($attribute, "JSStringIsEqualToUTF8CString(propertyName, \"${name}\")") if $hasDynamicProperties;

            my $conditionalString = conditionalString($attribute);

            push(@contents, "#if ${conditionalString}\n") if $conditionalString;
            push(@contents, "    if (${condition})\n");
            push(@contents, "        return ${getterName}(context, thisObject, propertyName, exception);\n");
            push(@contents, "#endif // ${conditionalString}\n") if $conditionalString;
            push(@contents, "\n");
        }
    }

    if (my @functions = @{$interface->operations}) {
        foreach my $function (@functions) {
            next unless $function->extendedAttributes->{"Dynamic"} or $function->extendedAttributes->{"MainWorldOnly"};

            my $name = $function->name;

            my $condition = "JSStringIsEqualToUTF8CString(propertyName, \"${name}\")" if $hasMainWorldOnlyProperties and !$hasDynamicProperties;
            $condition = &$generateCondition($function, "JSStringIsEqualToUTF8CString(propertyName, \"${name}\")") if $hasDynamicProperties;

            my $conditionalString = conditionalString($function);

            push(@contents, "#if ${conditionalString}\n") if $conditionalString;
            push(@contents, "    if (${condition})\n");
            push(@contents, "        return JSObjectMakeFunctionWithCallback(context, propertyName, ${name});\n");
            push(@contents, "#endif // ${conditionalString}\n") if $conditionalString;
            push(@contents, "\n");
        }
    }

    push(@contents, <<EOF);
    return JSValueMakeUndefined(context);
}
EOF

    return @contents;
}

1;
