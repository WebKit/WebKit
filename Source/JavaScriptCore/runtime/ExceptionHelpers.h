/*
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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

#include "ErrorInstance.h"
#include "Exception.h"
#include "JSObject.h"
#include "ThrowScope.h"

namespace JSC {

typedef JSObject* (*ErrorFactory)(JSGlobalObject*, const String&, ErrorInstance::SourceAppender);

String defaultSourceAppender(const String&, const String&, RuntimeType, ErrorInstance::SourceTextWhereErrorOccurred);

JSObject* createTerminatedExecutionException(VM*);
JS_EXPORT_PRIVATE bool isTerminatedExecutionException(VM&, Exception*);
JS_EXPORT_PRIVATE JSObject* createError(JSGlobalObject*, JSValue, const String&, ErrorInstance::SourceAppender);
JS_EXPORT_PRIVATE JSObject* createStackOverflowError(JSGlobalObject*);
JSObject* createUndefinedVariableError(JSGlobalObject*, const Identifier&);
JSObject* createTDZError(JSGlobalObject*);
JSObject* createNotAnObjectError(JSGlobalObject*, JSValue);
JSObject* createInvalidFunctionApplyParameterError(JSGlobalObject*, JSValue);
JSObject* createInvalidInParameterError(JSGlobalObject*, JSValue);
JSObject* createInvalidInstanceofParameterErrorNotFunction(JSGlobalObject*, JSValue);
JSObject* createInvalidInstanceofParameterErrorHasInstanceValueNotFunction(JSGlobalObject*, JSValue);
JSObject* createNotAConstructorError(JSGlobalObject*, JSValue);
JSObject* createNotAFunctionError(JSGlobalObject*, JSValue);
JSObject* createErrorForInvalidGlobalAssignment(JSGlobalObject*, const String&);
String errorDescriptionForValue(JSGlobalObject*, JSValue);

JS_EXPORT_PRIVATE Exception* throwOutOfMemoryError(JSGlobalObject*, ThrowScope&);
JS_EXPORT_PRIVATE Exception* throwStackOverflowError(JSGlobalObject*, ThrowScope&);
JS_EXPORT_PRIVATE Exception* throwTerminatedExecutionException(JSGlobalObject*, ThrowScope&);


class TerminatedExecutionError final : public JSNonFinalObject {
public:
    typedef JSNonFinalObject Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    static TerminatedExecutionError* create(VM& vm)
    {
        TerminatedExecutionError* error = new (NotNull, allocateCell<TerminatedExecutionError>(vm.heap)) TerminatedExecutionError(vm);
        error->finishCreation(vm);
        return error;
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    DECLARE_EXPORT_INFO;

private:
    explicit TerminatedExecutionError(VM& vm)
        : JSNonFinalObject(vm, vm.terminatedExecutionErrorStructure.get())
    {
    }

    static JSValue defaultValue(const JSObject*, JSGlobalObject*, PreferredPrimitiveType);

};

} // namespace JSC
