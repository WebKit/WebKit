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

#include "DeferTermination.h"
#include "ErrorInstance.h"
#include "Exception.h"
#include "JSObject.h"
#include "ThrowScope.h"

namespace JSC {

String defaultSourceAppender(const String&, StringView, RuntimeType, ErrorInstance::SourceTextWhereErrorOccurred);
String notAFunctionSourceAppender(const String&, StringView, RuntimeType, ErrorInstance::SourceTextWhereErrorOccurred);

String constructErrorMessage(JSGlobalObject*, JSValue, const String&);
JS_EXPORT_PRIVATE ErrorInstance* createError(JSGlobalObject*, JSValue, const String&, ErrorInstance::SourceAppender);
JS_EXPORT_PRIVATE ErrorInstance* createStackOverflowError(JSGlobalObject*);
ErrorInstance* createUndefinedVariableError(JSGlobalObject*, const Identifier&);
ErrorInstance* createTDZError(JSGlobalObject*);
ErrorInstance* createNotAnObjectError(JSGlobalObject*, JSValue);
ErrorInstance* createInvalidFunctionApplyParameterError(JSGlobalObject*, JSValue);
ErrorInstance* createInvalidInParameterError(JSGlobalObject*, JSValue);
ErrorInstance* createInvalidInstanceofParameterErrorNotFunction(JSGlobalObject*, JSValue);
ErrorInstance* createInvalidInstanceofParameterErrorHasInstanceValueNotFunction(JSGlobalObject*, JSValue);
ErrorInstance* createNotAConstructorError(JSGlobalObject*, JSValue);
ErrorInstance* createNotAFunctionError(JSGlobalObject*, JSValue);
ErrorInstance* createInvalidPrototypeError(JSGlobalObject*, JSValue);
ErrorInstance* createErrorForDuplicateGlobalVariableDeclaration(JSGlobalObject*, UniquedStringImpl*);
ErrorInstance* createErrorForInvalidGlobalFunctionDeclaration(JSGlobalObject*, const Identifier&);
ErrorInstance* createErrorForInvalidGlobalVarDeclaration(JSGlobalObject*, const Identifier&);
ErrorInstance* createInvalidPrivateNameError(JSGlobalObject*);
ErrorInstance* createRedefinedPrivateNameError(JSGlobalObject*);
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

} // namespace JSC
