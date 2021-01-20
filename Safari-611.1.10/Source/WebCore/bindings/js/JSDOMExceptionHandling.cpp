/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2020 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
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

#include "config.h"
#include "JSDOMExceptionHandling.h"

#include "CachedScript.h"
#include "DOMException.h"
#include "DOMWindow.h"
#include "JSDOMException.h"
#include "JSDOMPromiseDeferred.h"
#include "JSDOMWindow.h"
#include "ScriptExecutionContext.h"
#include <JavaScriptCore/ErrorHandlingScope.h>
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/ExceptionHelpers.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>

namespace WebCore {
using namespace JSC;

void reportException(JSGlobalObject* lexicalGlobalObject, JSValue exceptionValue, CachedScript* cachedScript)
{
    VM& vm = lexicalGlobalObject->vm();
    RELEASE_ASSERT(vm.currentThreadIsHoldingAPILock());
    auto* exception = jsDynamicCast<JSC::Exception*>(vm, exceptionValue);
    if (!exception) {
        exception = vm.lastException();
        if (!exception)
            exception = JSC::Exception::create(lexicalGlobalObject->vm(), exceptionValue, JSC::Exception::DoNotCaptureStack);
    }

    reportException(lexicalGlobalObject, exception, cachedScript);
}

String retrieveErrorMessage(JSGlobalObject& lexicalGlobalObject, VM& vm, JSValue exception, CatchScope& catchScope)
{
    // FIXME: <http://webkit.org/b/115087> Web Inspector: WebCore::reportException should not evaluate JavaScript handling exceptions
    // If this is a custom exception object, call toString on it to try and get a nice string representation for the exception.
    String errorMessage;
    if (auto* error = jsDynamicCast<ErrorInstance*>(vm, exception))
        errorMessage = error->sanitizedToString(&lexicalGlobalObject);
    else
        errorMessage = exception.toWTFString(&lexicalGlobalObject);

    // We need to clear any new exception that may be thrown in the toString() call above.
    // reportException() is not supposed to be making new exceptions.
    catchScope.clearException();
    vm.clearLastException();
    return errorMessage;
}

void reportException(JSGlobalObject* lexicalGlobalObject, JSC::Exception* exception, CachedScript* cachedScript, ExceptionDetails* exceptionDetails)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    RELEASE_ASSERT(vm.currentThreadIsHoldingAPILock());
    if (isTerminatedExecutionException(vm, exception))
        return;

    ErrorHandlingScope errorScope(lexicalGlobalObject->vm());

    auto callStack = Inspector::createScriptCallStackFromException(lexicalGlobalObject, exception);
    scope.clearException();
    vm.clearLastException();

    auto* globalObject = jsCast<JSDOMGlobalObject*>(lexicalGlobalObject);
    if (auto* window = jsDynamicCast<JSDOMWindow*>(vm, globalObject)) {
        if (!window->wrapped().isCurrentlyDisplayedInFrame())
            return;
    }

    int lineNumber = 0;
    int columnNumber = 0;
    String exceptionSourceURL;
    if (auto* callFrame = callStack->firstNonNativeCallFrame()) {
        lineNumber = callFrame->lineNumber();
        columnNumber = callFrame->columnNumber();
        exceptionSourceURL = callFrame->sourceURL();
    }

    auto errorMessage = retrieveErrorMessage(*lexicalGlobalObject, vm, exception->value(), scope);
    globalObject->scriptExecutionContext()->reportException(errorMessage, lineNumber, columnNumber, exceptionSourceURL, exception, callStack->size() ? callStack.ptr() : nullptr, cachedScript);

    if (exceptionDetails) {
        exceptionDetails->message = errorMessage;
        exceptionDetails->lineNumber = lineNumber;
        exceptionDetails->columnNumber = columnNumber;
        exceptionDetails->sourceURL = exceptionSourceURL;
    }
}

void reportCurrentException(JSGlobalObject* lexicalGlobalObject)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);
    auto* exception = scope.exception();
    scope.clearException();
    reportException(lexicalGlobalObject, exception);
}

JSValue createDOMException(JSGlobalObject* lexicalGlobalObject, ExceptionCode ec, const String& message)
{
    if (ec == ExistingExceptionError)
        return jsUndefined();

    // FIXME: Handle other WebIDL exception types.
    if (ec == TypeError) {
        if (message.isEmpty())
            return createTypeError(lexicalGlobalObject);
        return createTypeError(lexicalGlobalObject, message);
    }

    if (ec == RangeError) {
        if (message.isEmpty())
            return createRangeError(lexicalGlobalObject, "Bad value"_s);
        return createRangeError(lexicalGlobalObject, message);
    }

    if (ec == StackOverflowError)
        return createStackOverflowError(lexicalGlobalObject);
    if (ec == OutOfMemoryError)
        return createOutOfMemoryError(lexicalGlobalObject);

    // FIXME: All callers to createDOMException need to pass in the correct global object.
    // For now, we're going to assume the lexicalGlobalObject. Which is wrong in cases like this:
    // frames[0].document.createElement(null, null); // throws an exception which should have the subframe's prototypes.
    JSDOMGlobalObject* globalObject = deprecatedGlobalObjectForPrototype(lexicalGlobalObject);
    JSValue errorObject = toJS(lexicalGlobalObject, globalObject, DOMException::create(ec, message));
    
    ASSERT(errorObject);
    addErrorInfo(lexicalGlobalObject, asObject(errorObject), true);
    return errorObject;
}

JSValue createDOMException(JSGlobalObject& lexicalGlobalObject, Exception&& exception)
{
    return createDOMException(&lexicalGlobalObject, exception.code(), exception.releaseMessage());
}

void propagateExceptionSlowPath(JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& throwScope, Exception&& exception)
{
    throwScope.assertNoException();
    throwException(&lexicalGlobalObject, throwScope, createDOMException(lexicalGlobalObject, WTFMove(exception)));
}
    
static EncodedJSValue throwTypeError(JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope, const String& errorMessage)
{
    return throwVMTypeError(&lexicalGlobalObject, scope, errorMessage);
}

template<typename... StringTypes> static String makeArgumentTypeErrorMessage(unsigned argumentIndex, const char* argumentName, const char* interfaceName, const char* functionName, StringTypes ...strings)
{
    return makeString(
        "Argument ", argumentIndex + 1, " ('", argumentName, "') to ",
        functionName ? std::make_tuple(interfaceName, ".", functionName) : std::make_tuple("the ", interfaceName, " constructor"),
        " must be ", strings...
    );
}

void throwNotSupportedError(JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope, ASCIILiteral message)
{
    scope.assertNoException();
    throwException(&lexicalGlobalObject, scope, createDOMException(&lexicalGlobalObject, NotSupportedError, message));
}

void throwInvalidStateError(JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope, ASCIILiteral message)
{
    scope.assertNoException();
    throwException(&lexicalGlobalObject, scope, createDOMException(&lexicalGlobalObject, InvalidStateError, message));
}

void throwSecurityError(JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope, const String& message)
{
    scope.assertNoException();
    throwException(&lexicalGlobalObject, scope, createDOMException(&lexicalGlobalObject, SecurityError, message));
}

JSC::EncodedJSValue throwArgumentMustBeEnumError(JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope, unsigned argumentIndex, const char* argumentName, const char* functionInterfaceName, const char* functionName, const char* expectedValues)
{
    return throwVMTypeError(&lexicalGlobalObject, scope, makeArgumentTypeErrorMessage(argumentIndex, argumentName, functionInterfaceName, functionName, "one of: ", expectedValues));
}

JSC::EncodedJSValue throwArgumentMustBeFunctionError(JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope, unsigned argumentIndex, const char* argumentName, const char* interfaceName, const char* functionName)
{
    return throwVMTypeError(&lexicalGlobalObject, scope, makeArgumentTypeErrorMessage(argumentIndex, argumentName, interfaceName, functionName, "a function"));
}

JSC::EncodedJSValue throwArgumentMustBeObjectError(JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope, unsigned argumentIndex, const char* argumentName, const char* interfaceName, const char* functionName)
{
    return throwVMTypeError(&lexicalGlobalObject, scope, makeArgumentTypeErrorMessage(argumentIndex, argumentName, interfaceName, functionName, "an object"));
}

JSC::EncodedJSValue throwArgumentTypeError(JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope, unsigned argumentIndex, const char* argumentName, const char* functionInterfaceName, const char* functionName, const char* expectedType)
{
    return throwVMTypeError(&lexicalGlobalObject, scope, makeArgumentTypeErrorMessage(argumentIndex, argumentName, functionInterfaceName, functionName, "an instance of ", expectedType));
}

void throwAttributeTypeError(JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope, const char* interfaceName, const char* attributeName, const char* expectedType)
{
    throwTypeError(lexicalGlobalObject, scope, makeString("The ", interfaceName, '.', attributeName, " attribute must be an instance of ", expectedType));
}

JSC::EncodedJSValue throwRequiredMemberTypeError(JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope, const char* memberName, const char* dictionaryName, const char* expectedType)
{
    return throwVMTypeError(&lexicalGlobalObject, scope, makeString("Member ", dictionaryName, '.', memberName, " is required and must be an instance of ", expectedType));
}

JSC::EncodedJSValue throwConstructorScriptExecutionContextUnavailableError(JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope, const char* interfaceName)
{
    return throwVMError(&lexicalGlobalObject, scope, createReferenceError(&lexicalGlobalObject, makeString(interfaceName, " constructor associated execution context is unavailable")));
}

void throwSequenceTypeError(JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope)
{
    throwTypeError(lexicalGlobalObject, scope, "Value is not a sequence"_s);
}

void throwNonFiniteTypeError(JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope)
{
    throwTypeError(&lexicalGlobalObject, scope, "The provided value is non-finite"_s);
}

String makeGetterTypeErrorMessage(const char* interfaceName, const char* attributeName)
{
    return makeString("The ", interfaceName, '.', attributeName, " getter can only be used on instances of ", interfaceName);
}

JSC::EncodedJSValue throwGetterTypeError(JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope, const char* interfaceName, const char* attributeName)
{
    return throwVMGetterTypeError(&lexicalGlobalObject, scope, makeGetterTypeErrorMessage(interfaceName, attributeName));
}

JSC::EncodedJSValue rejectPromiseWithGetterTypeError(JSC::JSGlobalObject& lexicalGlobalObject, const char* interfaceName, const char* attributeName)
{
    return createRejectedPromiseWithTypeError(lexicalGlobalObject, makeGetterTypeErrorMessage(interfaceName, attributeName), RejectedPromiseWithTypeErrorCause::NativeGetter);
}

bool throwSetterTypeError(JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope, const char* interfaceName, const char* attributeName)
{
    throwTypeError(lexicalGlobalObject, scope, makeString("The ", interfaceName, '.', attributeName, " setter can only be used on instances of ", interfaceName));
    return false;
}

String makeThisTypeErrorMessage(const char* interfaceName, const char* functionName)
{
    return makeString("Can only call ", interfaceName, '.', functionName, " on instances of ", interfaceName);
}

EncodedJSValue throwThisTypeError(JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope, const char* interfaceName, const char* functionName)
{
    return throwTypeError(lexicalGlobalObject, scope, makeThisTypeErrorMessage(interfaceName, functionName));
}

JSC::EncodedJSValue rejectPromiseWithThisTypeError(DeferredPromise& promise, const char* interfaceName, const char* methodName)
{
    promise.reject(TypeError, makeThisTypeErrorMessage(interfaceName, methodName));
    return JSValue::encode(jsUndefined());
}

JSC::EncodedJSValue rejectPromiseWithThisTypeError(JSC::JSGlobalObject& lexicalGlobalObject, const char* interfaceName, const char* methodName)
{
    return createRejectedPromiseWithTypeError(lexicalGlobalObject, makeThisTypeErrorMessage(interfaceName, methodName), RejectedPromiseWithTypeErrorCause::InvalidThis);
}

void throwDOMSyntaxError(JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope, ASCIILiteral message)
{
    scope.assertNoException();
    throwException(&lexicalGlobalObject, scope, createDOMException(&lexicalGlobalObject, SyntaxError, message));
}

void throwDataCloneError(JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope)
{
    scope.assertNoException();
    throwException(&lexicalGlobalObject, scope, createDOMException(&lexicalGlobalObject, DataCloneError));
}

} // namespace WebCore
