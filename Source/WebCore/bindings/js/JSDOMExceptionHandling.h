/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2017 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *  Copyright (C) 2009 Google, Inc. All rights reserved.
 *  Copyright (C) 2012 Ericsson AB. All rights reserved.
 *  Copyright (C) 2013 Michael Pruett <michael@68k.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include "ExceptionOr.h"
#include <JavaScriptCore/ThrowScope.h>

namespace JSC {
class CatchScope;
}

namespace WebCore {

class CachedScript;
class DeferredPromise;
class JSDOMGlobalObject;

struct ExceptionDetails {
    String message;
    int lineNumber { 0 };
    int columnNumber { 0 };
    String sourceURL;
};

void throwAttributeTypeError(JSC::ExecState&, JSC::ThrowScope&, const char* interfaceName, const char* attributeName, const char* expectedType);
WEBCORE_EXPORT bool throwSetterTypeError(JSC::ExecState&, JSC::ThrowScope&, const char* interfaceName, const char* attributeName);

void throwDataCloneError(JSC::ExecState&, JSC::ThrowScope&);
void throwDOMSyntaxError(JSC::ExecState&, JSC::ThrowScope&, ASCIILiteral); // Not the same as a JavaScript syntax error.
void throwInvalidStateError(JSC::ExecState&, JSC::ThrowScope&, ASCIILiteral);
WEBCORE_EXPORT void throwNonFiniteTypeError(JSC::ExecState&, JSC::ThrowScope&);
void throwNotSupportedError(JSC::ExecState&, JSC::ThrowScope&, ASCIILiteral);
void throwSecurityError(JSC::ExecState&, JSC::ThrowScope&, const String& message);
WEBCORE_EXPORT void throwSequenceTypeError(JSC::ExecState&, JSC::ThrowScope&);

WEBCORE_EXPORT JSC::EncodedJSValue throwArgumentMustBeEnumError(JSC::ExecState&, JSC::ThrowScope&, unsigned argumentIndex, const char* argumentName, const char* functionInterfaceName, const char* functionName, const char* expectedValues);
WEBCORE_EXPORT JSC::EncodedJSValue throwArgumentMustBeFunctionError(JSC::ExecState&, JSC::ThrowScope&, unsigned argumentIndex, const char* argumentName, const char* functionInterfaceName, const char* functionName);
WEBCORE_EXPORT JSC::EncodedJSValue throwArgumentTypeError(JSC::ExecState&, JSC::ThrowScope&, unsigned argumentIndex, const char* argumentName, const char* functionInterfaceName, const char* functionName, const char* expectedType);
WEBCORE_EXPORT JSC::EncodedJSValue throwRequiredMemberTypeError(JSC::ExecState&, JSC::ThrowScope&, const char* memberName, const char* dictionaryName, const char* expectedType);
JSC::EncodedJSValue throwConstructorScriptExecutionContextUnavailableError(JSC::ExecState&, JSC::ThrowScope&, const char* interfaceName);

String makeGetterTypeErrorMessage(const char* interfaceName, const char* attributeName);
String makeThisTypeErrorMessage(const char* interfaceName, const char* attributeName);

WEBCORE_EXPORT JSC::EncodedJSValue throwGetterTypeError(JSC::ExecState&, JSC::ThrowScope&, const char* interfaceName, const char* attributeName);
WEBCORE_EXPORT JSC::EncodedJSValue throwThisTypeError(JSC::ExecState&, JSC::ThrowScope&, const char* interfaceName, const char* functionName);

WEBCORE_EXPORT JSC::EncodedJSValue rejectPromiseWithGetterTypeError(JSC::ExecState&, const char* interfaceName, const char* attributeName);
WEBCORE_EXPORT JSC::EncodedJSValue rejectPromiseWithThisTypeError(DeferredPromise&, const char* interfaceName, const char* operationName);
WEBCORE_EXPORT JSC::EncodedJSValue rejectPromiseWithThisTypeError(JSC::ExecState&, const char* interfaceName, const char* operationName);

String retrieveErrorMessage(JSC::ExecState&, JSC::VM&, JSC::JSValue exception, JSC::CatchScope&);
WEBCORE_EXPORT void reportException(JSC::ExecState*, JSC::JSValue exception, CachedScript* = nullptr);
WEBCORE_EXPORT void reportException(JSC::ExecState*, JSC::Exception*, CachedScript* = nullptr, ExceptionDetails* = nullptr);
void reportCurrentException(JSC::ExecState*);

JSC::JSValue createDOMException(JSC::ExecState&, Exception&&);
JSC::JSValue createDOMException(JSC::ExecState*, ExceptionCode, const String& = emptyString());

// Convert a DOM implementation exception into a JavaScript exception in the execution state.
WEBCORE_EXPORT void propagateExceptionSlowPath(JSC::ExecState&, JSC::ThrowScope&, Exception&&);

ALWAYS_INLINE void propagateException(JSC::ExecState& state, JSC::ThrowScope& throwScope, Exception&& exception)
{
    if (throwScope.exception())
        return;
    propagateExceptionSlowPath(state, throwScope, WTFMove(exception));
}

inline void propagateException(JSC::ExecState& state, JSC::ThrowScope& throwScope, ExceptionOr<void>&& value)
{
    if (UNLIKELY(value.hasException()))
        propagateException(state, throwScope, value.releaseException());
}

} // namespace WebCore
