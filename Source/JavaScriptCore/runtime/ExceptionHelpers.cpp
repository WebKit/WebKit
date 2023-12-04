/*
 * Copyright (C) 2008-2023 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#include "config.h"
#include "ExceptionHelpers.h"

#include "CatchScope.h"
#include "ErrorHandlingScope.h"
#include "Exception.h"
#include "JSCInlines.h"
#include "RuntimeType.h"
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringView.h>

namespace JSC {

JSObject* createStackOverflowError(JSGlobalObject* globalObject)
{
    auto* error = createRangeError(globalObject, "Maximum call stack size exceeded."_s);
    jsCast<ErrorInstance*>(error)->setStackOverflowError();
    return error;
}

JSObject* createUndefinedVariableError(JSGlobalObject* globalObject, const Identifier& ident)
{
    if (ident.isPrivateName())
        return createReferenceError(globalObject, makeString("Can't find private variable: PrivateSymbol."_s, ident.string()));
    return createReferenceError(globalObject, makeString("Can't find variable: "_s, ident.string()));
}
    
String errorDescriptionForValue(JSGlobalObject* globalObject, JSValue v)
{
    if (v.isString()) {
        String string = asString(v)->value(globalObject);
        if (!string)
            return string;
        return tryMakeString('"', string, '"');
    }

    if (v.isSymbol()) {
        auto expectedDescription = asSymbol(v)->tryGetDescriptiveString();
        if (expectedDescription)
            return expectedDescription.value();
        ASSERT(expectedDescription.error() == ErrorTypeWithExtension::OutOfMemoryError);
        return String("Symbol"_s);
    }
    if (v.isObject()) {
        VM& vm = globalObject->vm();
        JSObject* object = asObject(v);
        if (object->isCallable())
            return vm.smallStrings.functionString()->value(globalObject);
        return JSObject::calculatedClassName(object);
    }
    return v.toString(globalObject)->value(globalObject);
}
    
static StringView clampErrorMessage(const String& originalMessage)
{
    // Hopefully this is sufficiently long. Note, this is the length of the string not the number of bytes used.
    constexpr unsigned maxLength = 2 * KB;
    return StringView(originalMessage).left(maxLength);
}

static String defaultApproximateSourceError(const String& originalMessage, StringView sourceText)
{
    return makeString(clampErrorMessage(originalMessage), " (near '..."_s, sourceText, "...')"_s);
}

String defaultSourceAppender(const String& originalMessage, StringView sourceText, RuntimeType, ErrorInstance::SourceTextWhereErrorOccurred occurrence)
{
    if (occurrence == ErrorInstance::FoundApproximateSource)
        return defaultApproximateSourceError(originalMessage, sourceText);

    ASSERT(occurrence == ErrorInstance::FoundExactSource);
    return makeString(clampErrorMessage(originalMessage), " (evaluating '"_s, sourceText, "')"_s);
}

static StringView functionCallBase(StringView sourceText)
{ 
    // This function retrieves the 'foo.bar' substring from 'foo.bar(baz)'.
    // FIXME: This function has simple processing of /* */ style comments.
    // It doesn't properly handle embedded comments of string literals that contain
    // parenthesis or comment constructs, e.g. foo.bar("/abc\)*/").
    // https://bugs.webkit.org/show_bug.cgi?id=146304

    unsigned sourceLength = sourceText.length();
    unsigned idx = sourceLength - 1;
    if (sourceLength < 2 || sourceText[idx] != ')') {
        // For function calls that have many new lines in between their open parenthesis
        // and their closing parenthesis, the text range passed into the message appender 
        // will not include the text in between these parentheses, it will just be the desired
        // text that precedes the parentheses.
        return String();
    }

    unsigned parenStack = 1;
    bool isInMultiLineComment = false;
    idx -= 1;
    // Note that we're scanning text right to left instead of the more common left to right, 
    // so syntax detection is backwards.
    while (parenStack && idx) {
        UChar curChar = sourceText[idx];
        if (isInMultiLineComment) {
            if (curChar == '*' && sourceText[idx - 1] == '/') {
                isInMultiLineComment = false;
                --idx;
            }
        } else if (curChar == '(')
            --parenStack;
        else if (curChar == ')')
            ++parenStack;
        else if (curChar == '/' && sourceText[idx - 1] == '*') {
            isInMultiLineComment = true;
            --idx;
        }

        if (idx)
            --idx;
    }

    if (parenStack) {
        // As noted in the FIXME at the top of this function, there are bugs
        // in the above string processing. This algorithm is mostly best effort
        // and it works for most JS text in practice. However, if we determine
        // that the algorithm failed, we should just return the empty value.
        return String();
    }

    // Don't display the ?. of an optional call.
    if (idx > 1 && sourceText[idx] == '.' && sourceText[idx - 1] == '?')
        idx -= 2;

    return sourceText.left(idx + 1);
}

static String notAFunctionSourceAppender(const String& originalMessage, StringView sourceText, RuntimeType type, ErrorInstance::SourceTextWhereErrorOccurred occurrence)
{
    ASSERT(type != TypeFunction);

    if (occurrence == ErrorInstance::FoundApproximateSource)
        return defaultApproximateSourceError(originalMessage, sourceText);

    ASSERT(occurrence == ErrorInstance::FoundExactSource);
    auto notAFunctionIndex = originalMessage.reverseFind("is not a function"_s);
    RELEASE_ASSERT(notAFunctionIndex != notFound);
    StringView displayValue;
    if (originalMessage.is8Bit()) 
        displayValue = StringView(originalMessage.characters8(), notAFunctionIndex - 1);
    else
        displayValue = StringView(originalMessage.characters16(), notAFunctionIndex - 1);

    StringView base = functionCallBase(sourceText);
    if (!base)
        return defaultApproximateSourceError(originalMessage, sourceText);
    StringBuilder builder(StringBuilder::OverflowHandler::RecordOverflow);
    builder.append(base, " is not a function. (In '", sourceText, "', '", base, "' is ");
    if (type == TypeSymbol)
        builder.append("a Symbol");
    else {
        if (type == TypeObject)
            builder.append("an instance of ");
        builder.append(displayValue);
    }
    builder.append(')');

    if (builder.hasOverflowed())
        return "object is not a function."_s;

    return builder.toString();
}

static String invalidParameterInSourceAppender(const String& originalMessage, StringView sourceText, RuntimeType type, ErrorInstance::SourceTextWhereErrorOccurred occurrence)
{
    ASSERT_UNUSED(type, type != TypeObject);

    if (occurrence == ErrorInstance::FoundApproximateSource)
        return defaultApproximateSourceError(originalMessage, sourceText);

    ASSERT(occurrence == ErrorInstance::FoundExactSource);
    auto inIndex = sourceText.reverseFind("in"_s);
    if (inIndex == notFound) {
        // This should basically never happen, since JS code must use the literal
        // text "in" for the `in` operation. However, if we fail to find "in"
        // for any reason, just fail gracefully.
        return originalMessage;
    }
    if (sourceText.find("in"_s) != inIndex)
        return makeString(originalMessage, " (evaluating '"_s, sourceText, "')"_s);

    static constexpr unsigned inLength = 2;
    auto rightHandSide = sourceText.substring(inIndex + inLength).trim(deprecatedIsSpaceOrNewline);
    return makeString(rightHandSide, " is not an Object. (evaluating '"_s, sourceText, "')"_s);
}

inline String invalidParameterInstanceofSourceAppender(const String& content, const String& originalMessage, StringView sourceText, RuntimeType, ErrorInstance::SourceTextWhereErrorOccurred occurrence)
{
    if (occurrence == ErrorInstance::FoundApproximateSource)
        return defaultApproximateSourceError(originalMessage, sourceText);

    ASSERT(occurrence == ErrorInstance::FoundExactSource);
    auto instanceofIndex = sourceText.reverseFind("instanceof"_s);
    // This can happen when Symbol.hasInstance function is directly called.
    if (instanceofIndex == notFound)
        return originalMessage;

    if (sourceText.find("instanceof"_s) != instanceofIndex)
        return makeString(originalMessage, " (evaluating '"_s, sourceText, "')"_s);

    static constexpr unsigned instanceofLength = 10;
    auto rightHandSide = sourceText.substring(instanceofIndex + instanceofLength).trim(deprecatedIsSpaceOrNewline);
    return makeString(rightHandSide, content, ". (evaluating '"_s, sourceText, "')"_s);
}

static String invalidParameterInstanceofNotFunctionSourceAppender(const String& originalMessage, StringView sourceText, RuntimeType runtimeType, ErrorInstance::SourceTextWhereErrorOccurred occurrence)
{
    return invalidParameterInstanceofSourceAppender(" is not a function"_s, originalMessage, sourceText, runtimeType, occurrence);
}

static String invalidParameterInstanceofhasInstanceValueNotFunctionSourceAppender(const String& originalMessage, StringView sourceText, RuntimeType runtimeType, ErrorInstance::SourceTextWhereErrorOccurred occurrence)
{
    return invalidParameterInstanceofSourceAppender("[Symbol.hasInstance] is not a function, undefined, or null"_s, originalMessage, sourceText, runtimeType, occurrence);
}

static String invalidPrototypeSourceAppender(const String& originalMessage, StringView sourceText, RuntimeType, ErrorInstance::SourceTextWhereErrorOccurred occurrence)
{
    if (occurrence == ErrorInstance::FoundApproximateSource)
        return defaultApproximateSourceError(originalMessage, sourceText);

    auto extendsIndex = sourceText.reverseFind("extends"_s);
    if (extendsIndex == notFound || sourceText.find("extends"_s) != extendsIndex)
        return makeString(originalMessage, " (evaluating '"_s, sourceText, "')"_s);

    return "The value of the superclass's prototype property is not an object or null."_s;
}

JSObject* createError(JSGlobalObject* globalObject, JSValue value, const String& message, ErrorInstance::SourceAppender appender)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    String valueDescription = errorDescriptionForValue(globalObject, value);
    if (scope.exception() || !valueDescription) {
        // When we see an exception, we're not returning immediately because
        // we're in a CatchScope, i.e. no exceptions are thrown past this scope.
        // We're using a CatchScope because the contract for createError() is
        // that it only creates an error object; it doesn't throw it.
        scope.clearException();
        return createOutOfMemoryError(globalObject);
    }
    String errorMessage = tryMakeString(valueDescription, ' ', message);
    if (!errorMessage)
        return createOutOfMemoryError(globalObject);
    scope.assertNoException();
    JSObject* exception = createTypeError(globalObject, errorMessage, appender, runtimeTypeForValue(value));
    ASSERT(exception->isErrorInstance());

    return exception;
}

JSObject* createInvalidFunctionApplyParameterError(JSGlobalObject* globalObject, JSValue value)
{
    return createTypeError(globalObject, "second argument to Function.prototype.apply must be an Array-like object"_s, defaultSourceAppender, runtimeTypeForValue(value));
}

JSObject* createInvalidInParameterError(JSGlobalObject* globalObject, JSValue value)
{
    return createError(globalObject, value, "is not an Object."_s, invalidParameterInSourceAppender);
}

JSObject* createInvalidInstanceofParameterErrorNotFunction(JSGlobalObject* globalObject, JSValue value)
{
    return createError(globalObject, value, " is not a function"_s, invalidParameterInstanceofNotFunctionSourceAppender);
}

JSObject* createInvalidInstanceofParameterErrorHasInstanceValueNotFunction(JSGlobalObject* globalObject, JSValue value)
{
    return createError(globalObject, value, "[Symbol.hasInstance] is not a function, undefined, or null"_s, invalidParameterInstanceofhasInstanceValueNotFunctionSourceAppender);
}

JSObject* createNotAConstructorError(JSGlobalObject* globalObject, JSValue value)
{
    return createError(globalObject, value, "is not a constructor"_s, defaultSourceAppender);
}

JSObject* createNotAFunctionError(JSGlobalObject* globalObject, JSValue value)
{
    return createError(globalObject, value, "is not a function"_s, notAFunctionSourceAppender);
}

JSObject* createNotAnObjectError(JSGlobalObject* globalObject, JSValue value)
{
    return createError(globalObject, value, "is not an object"_s, defaultSourceAppender);
}

JSObject* createInvalidPrototypeError(JSGlobalObject* globalObject, JSValue value)
{
    return createError(globalObject, value, "is not an object or null"_s, invalidPrototypeSourceAppender);
}

JSObject* createErrorForInvalidGlobalAssignment(JSGlobalObject* globalObject, const String& propertyName)
{
    return createReferenceError(globalObject, makeString("Strict mode forbids implicit creation of global property '"_s, propertyName, '\''));
}

JSObject* createTDZError(JSGlobalObject* globalObject)
{
    return createReferenceError(globalObject, "Cannot access uninitialized variable."_s);
}

JSObject* createInvalidPrivateNameError(JSGlobalObject* globalObject)
{
    return createTypeError(globalObject, "Cannot access invalid private field"_s, defaultSourceAppender, TypeNothing);
}

JSObject* createRedefinedPrivateNameError(JSGlobalObject* globalObject)
{
    return createTypeError(globalObject, "Cannot redefine existing private field"_s, defaultSourceAppender, TypeNothing);
}

JSObject* createPrivateMethodAccessError(JSGlobalObject* globalObject)
{
    return createTypeError(globalObject, "Cannot access private method or acessor"_s, defaultSourceAppender, TypeNothing);
}

JSObject* createReinstallPrivateMethodError(JSGlobalObject* globalObject)
{
    return createTypeError(globalObject, "Cannot install same private methods on object more than once"_s, defaultSourceAppender, TypeNothing);
}

Exception* throwOutOfMemoryError(JSGlobalObject* globalObject, ThrowScope& scope)
{
    return throwException(globalObject, scope, createOutOfMemoryError(globalObject));
}

Exception* throwOutOfMemoryError(JSGlobalObject* globalObject, ThrowScope& scope, const String& message)
{
    return throwException(globalObject, scope, createOutOfMemoryError(globalObject, message));
}

Exception* throwStackOverflowError(JSGlobalObject* globalObject, ThrowScope& scope)
{
    VM& vm = globalObject->vm();
    ErrorHandlingScope errorScope(vm);
    return throwException(globalObject, scope, createStackOverflowError(globalObject));
}

} // namespace JSC
