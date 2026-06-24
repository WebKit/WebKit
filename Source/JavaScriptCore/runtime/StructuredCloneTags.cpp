/*
 * Copyright (C) 2009-2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StructuredCloneTags.h"

#include "CommonIdentifiers.h"
#include "ErrorInstance.h"
#include "ExceptionScope.h"
#include "JSCJSValuePropertyInlines.h"
#include "JSGlobalObject.h"
#include "JSObjectInlines.h"
#include "PropertyDescriptor.h"
#include "ThrowScope.h"

namespace JSC {

std::optional<ErrorInformation> extractErrorInformationFromErrorInstance(JSGlobalObject* lexicalGlobalObject, ErrorInstance& errorInstance)
{
    ASSERT(lexicalGlobalObject);
    auto& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto errorTypeValue = errorInstance.get(lexicalGlobalObject, vm.propertyNames->name);
    RETURN_IF_EXCEPTION(scope, std::nullopt);
    String errorTypeString = errorTypeValue.toWTFString(lexicalGlobalObject);
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    PropertyDescriptor messageDescriptor;
    String message;
    if (errorInstance.getOwnPropertyDescriptor(lexicalGlobalObject, vm.propertyNames->message, messageDescriptor) && messageDescriptor.isDataDescriptor()) {
        EXCEPTION_ASSERT(!scope.exception());
        message = messageDescriptor.value().toWTFString(lexicalGlobalObject);
    }
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    PropertyDescriptor lineDescriptor;
    unsigned line = 0;
    if (errorInstance.getOwnPropertyDescriptor(lexicalGlobalObject, vm.propertyNames->line, lineDescriptor) && lineDescriptor.isDataDescriptor()) {
        EXCEPTION_ASSERT(!scope.exception());
        line = lineDescriptor.value().toNumber(lexicalGlobalObject);
    }
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    PropertyDescriptor columnDescriptor;
    unsigned column = 0;
    if (errorInstance.getOwnPropertyDescriptor(lexicalGlobalObject, vm.propertyNames->column, columnDescriptor) && columnDescriptor.isDataDescriptor()) {
        EXCEPTION_ASSERT(!scope.exception());
        column = columnDescriptor.value().toNumber(lexicalGlobalObject);
    }
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    PropertyDescriptor sourceURLDescriptor;
    String sourceURL;
    if (errorInstance.getOwnPropertyDescriptor(lexicalGlobalObject, vm.propertyNames->sourceURL, sourceURLDescriptor) && sourceURLDescriptor.isDataDescriptor()) {
        EXCEPTION_ASSERT(!scope.exception());
        sourceURL = sourceURLDescriptor.value().toWTFString(lexicalGlobalObject);
    }
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    PropertyDescriptor stackDescriptor;
    String stack;
    if (errorInstance.getOwnPropertyDescriptor(lexicalGlobalObject, vm.propertyNames->stack, stackDescriptor) && stackDescriptor.isDataDescriptor()) {
        EXCEPTION_ASSERT(!scope.exception());
        stack = stackDescriptor.value().toWTFString(lexicalGlobalObject);
    }
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    PropertyDescriptor causeDescriptor;
    String cause;
    if (errorInstance.getOwnPropertyDescriptor(lexicalGlobalObject, vm.propertyNames->cause, causeDescriptor) && causeDescriptor.isDataDescriptor()) {
        EXCEPTION_ASSERT(!scope.exception());
        cause = causeDescriptor.value().toWTFString(lexicalGlobalObject);
    }
    RETURN_IF_EXCEPTION(scope, std::nullopt);

    return { ErrorInformation { errorTypeString, message, line, column, sourceURL, stack, cause } };
}

} // namespace JSC

namespace WTF {

void printInternal(PrintStream& out, JSC::SerializationTag tag)
{
    auto tagName = JSC::name(tag);
    if (tagName[0U] != '<')
        out.print(tagName);
    else
        out.print("<unknown tag "_s, static_cast<unsigned>(tag), ">"_s);
}

} // namespace WTF
