/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
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

#pragma once

#include <JavaScriptCore/DeferTermination.h>
#include <JavaScriptCore/Error.h>
#include <JavaScriptCore/ErrorInstance.h>
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/ThrowScope.h>
#include <wtf/text/MakeString.h>

namespace JSC {

typedef JSObject* (*ErrorFactory)(JSGlobalObject*, const String&, ErrorInstance::SourceAppender);

String defaultSourceAppender(const String&, StringView, RuntimeType, ErrorInstance::SourceTextWhereErrorOccurred);
String notAFunctionSourceAppender(const String&, StringView, RuntimeType, ErrorInstance::SourceTextWhereErrorOccurred);

String constructErrorMessage(JSGlobalObject*, JSValue, const String&);
JS_EXPORT_PRIVATE JSObject* createError(JSGlobalObject*, JSValue, const String&, ErrorInstance::SourceAppender);
JS_EXPORT_PRIVATE JSObject* createStackOverflowError(JSGlobalObject*);
JSObject* createUndefinedVariableError(JSGlobalObject*, const Identifier&);
JSObject* createTDZError(JSGlobalObject*, StringView);
JSObject* createTDZError(JSGlobalObject*);
JS_EXPORT_PRIVATE JSObject* createNotAnObjectError(JSGlobalObject*, JSValue);
JSObject* createInvalidFunctionApplyParameterError(JSGlobalObject*, JSValue);
JSObject* createInvalidInParameterError(JSGlobalObject*, JSValue);
JSObject* createInvalidInstanceofParameterErrorNotFunction(JSGlobalObject*, JSValue);
JSObject* createInvalidInstanceofParameterErrorHasInstanceValueNotFunction(JSGlobalObject*, JSValue);
JSObject* createNotAConstructorError(JSGlobalObject*, JSValue);
JSObject* createNotAFunctionError(JSGlobalObject*, JSValue);
JSObject* createInvalidPrototypeError(JSGlobalObject*, JSValue);
JSObject* createErrorForDuplicateGlobalVariableDeclaration(JSGlobalObject*, UniquedStringImpl*);
JSObject* createErrorForInvalidGlobalFunctionDeclaration(JSGlobalObject*, const Identifier&);
JSObject* createErrorForInvalidGlobalVarDeclaration(JSGlobalObject*, const Identifier&);
JSObject* createInvalidPrivateNameError(JSGlobalObject*);
JSObject* createRedefinedPrivateNameError(JSGlobalObject*);
String errorDescriptionForValue(JSGlobalObject*, JSValue);

JS_EXPORT_PRIVATE Exception* throwOutOfMemoryError(JSGlobalObject*, ThrowScope&);
JS_EXPORT_PRIVATE Exception* throwOutOfMemoryError(JSGlobalObject*, ThrowScope&, const String&);
JS_EXPORT_PRIVATE Exception* throwStackOverflowError(JSGlobalObject*, ThrowScope&);

#if ASSERT_ENABLED

#define DEFER_TERMINATION_AND_ASSERT(vm, assertion, ...) do { \
        JSC::DeferTerminationForAWhile deferScope(vm); \
        ASSERT(assertion, __VA_ARGS__); \
    } while (false)

#define DEFER_TERMINATION_AND_ASSERT_WITH_MESSAGE(vm, assertion, ...) do { \
        JSC::DeferTerminationForAWhile deferScope(vm); \
        ASSERT_WITH_MESSAGE(assertion, __VA_ARGS__); \
    } while (false)

#else

#define DEFER_TERMINATION_AND_ASSERT(vm, assertion, ...) UNUSED_PARAM(vm)
#define DEFER_TERMINATION_AND_ASSERT_WITH_MESSAGE(vm, assertion, ...) UNUSED_PARAM(vm)

#endif // ASSERT_ENABLED

#define DEFER_TERMINATION_AND_RELEASE_ASSERT(vm, assertion, ...) do { \
        JSC::DeferTerminationForAWhile deferScope(vm); \
        RELEASE_ASSERT(assertion, __VA_ARGS__); \
    } while (false)

#define DEFER_TERMINATION_AND_RELEASE_ASSERT_WITH_MESSAGE(vm, assertion, ...) do { \
        JSC::DeferTerminationForAWhile deferScope(vm); \
        RELEASE_ASSERT_WITH_MESSAGE(assertion, __VA_ARGS__); \
    } while (false)

// Defined here rather than in JSCJSValue.h because they need throwException/createRangeError/createNotAnObjectError.
// https://tc39.es/ecma262/#sec-toindex
inline uint64_t JSValue::toIndex(JSGlobalObject* globalObject, ASCIILiteral errorName) const
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (isInt32()) {
        int32_t integer = asInt32();
        if (integer < 0) [[unlikely]] {
            throwException(globalObject, scope, createRangeError(globalObject, makeString(errorName, " cannot be negative"_s)));
            return 0;
        }
        return integer;
    }

    double d = toIntegerOrInfinity(globalObject);
    RETURN_IF_EXCEPTION(scope, 0);
    if (d < 0) [[unlikely]] {
        throwException(globalObject, scope, createRangeError(globalObject, makeString(errorName, " cannot be negative"_s)));
        return 0;
    }

    if (d > maxSafeInteger()) [[unlikely]] {
        throwException(globalObject, scope, createRangeError(globalObject, makeString(errorName, " larger than (2 ** 53) - 1"_s)));
        return 0;
    }

    RELEASE_AND_RETURN(scope, d);
}

ALWAYS_INLINE bool JSValue::requireObjectCoercible(JSGlobalObject* globalObject) const
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!isUndefinedOrNull())
        return true;
    throwException(globalObject, scope, createNotAnObjectError(globalObject, *this));
    return false;
}

} // namespace JSC
