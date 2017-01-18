/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2011, 2013, 2016 Apple Inc. All rights reserved.
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
#include "JSDOMBinding.h"

#include "CachedScript.h"
#include "CommonVM.h"
#include "DOMConstructorWithDocument.h"
#include "ExceptionCode.h"
#include "ExceptionCodeDescription.h"
#include "ExceptionHeaders.h"
#include "ExceptionInterfaces.h"
#include "Frame.h"
#include "HTMLParserIdioms.h"
#include "IDBDatabaseException.h"
#include "JSDOMConvert.h"
#include "JSDOMPromise.h"
#include "JSDOMWindowCustom.h"
#include "JSExceptionBase.h"
#include "SecurityOrigin.h"
#include <bytecode/CodeBlock.h>
#include <inspector/ScriptCallStack.h>
#include <inspector/ScriptCallStackFactory.h>
#include <runtime/DateInstance.h>
#include <runtime/Error.h>
#include <runtime/ErrorHandlingScope.h>
#include <runtime/ErrorInstance.h>
#include <runtime/Exception.h>
#include <runtime/ExceptionHelpers.h>
#include <runtime/JSFunction.h>
#include <stdarg.h>
#include <wtf/MathExtras.h>
#include <wtf/unicode/CharacterNames.h>
#include <wtf/text/StringBuilder.h>

using namespace JSC;
using namespace Inspector;

namespace WebCore {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(DOMConstructorObject);
STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(DOMConstructorWithDocument);

void addImpureProperty(const AtomicString& propertyName)
{
    commonVM().addImpureProperty(propertyName);
}

JSValue jsOwnedStringOrNull(ExecState* exec, const String& s)
{
    if (s.isNull())
        return jsNull();
    return jsOwnedString(exec, s);
}

JSValue jsStringOrUndefined(ExecState* exec, const String& s)
{
    if (s.isNull())
        return jsUndefined();
    return jsStringWithCache(exec, s);
}

JSValue jsString(ExecState* exec, const URL& url)
{
    return jsStringWithCache(exec, url.string());
}

JSValue jsStringOrUndefined(ExecState* exec, const URL& url)
{
    if (url.isNull())
        return jsUndefined();
    return jsStringWithCache(exec, url.string());
}

static inline String stringToByteString(ExecState& state, JSC::ThrowScope& scope, String&& string)
{
    if (!string.containsOnlyLatin1()) {
        throwTypeError(&state, scope);
        return { };
    }

    return string;
}

String identifierToByteString(ExecState& state, const Identifier& identifier)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String string = identifier.string();
    return stringToByteString(state, scope, WTFMove(string));
}

String valueToByteString(ExecState& state, JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String string = value.toWTFString(&state);
    RETURN_IF_EXCEPTION(scope, { });

    return stringToByteString(state, scope, WTFMove(string));
}

static inline bool hasUnpairedSurrogate(StringView string)
{
    // Fast path for 8-bit strings; they can't have any surrogates.
    if (string.is8Bit())
        return false;
    for (auto codePoint : string.codePoints()) {
        if (U_IS_SURROGATE(codePoint))
            return true;
    }
    return false;
}

static inline String stringToUSVString(String&& string)
{
    // Fast path for the case where there are no unpaired surrogates.
    if (!hasUnpairedSurrogate(string))
        return string;

    // Slow path: http://heycam.github.io/webidl/#dfn-obtain-unicode
    // Replaces unpaired surrogates with the replacement character.
    StringBuilder result;
    result.reserveCapacity(string.length());
    StringView view { string };
    for (auto codePoint : view.codePoints()) {
        if (U_IS_SURROGATE(codePoint))
            result.append(replacementCharacter);
        else
            result.append(codePoint);
    }
    return result.toString();
}

String identifierToUSVString(ExecState&, const Identifier& identifier)
{
    String string = identifier.string();
    return stringToUSVString(WTFMove(string));
}

String valueToUSVString(ExecState& state, JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String string = value.toWTFString(&state);
    RETURN_IF_EXCEPTION(scope, { });

    return stringToUSVString(WTFMove(string));
}

JSValue jsDate(ExecState* exec, double value)
{
    return DateInstance::create(exec->vm(), exec->lexicalGlobalObject()->dateStructure(), value);
}

double valueToDate(ExecState* exec, JSValue value)
{
    if (value.isNumber())
        return value.asNumber();
    if (!value.inherits(DateInstance::info()))
        return std::numeric_limits<double>::quiet_NaN();
    return static_cast<DateInstance*>(value.toObject(exec))->internalNumber();
}

void reportException(ExecState* exec, JSValue exceptionValue, CachedScript* cachedScript)
{
    VM& vm = exec->vm();
    RELEASE_ASSERT(vm.currentThreadIsHoldingAPILock());
    auto* exception = jsDynamicDowncast<JSC::Exception*>(exceptionValue);
    if (!exception) {
        exception = vm.lastException();
        if (!exception)
            exception = JSC::Exception::create(exec->vm(), exceptionValue, JSC::Exception::DoNotCaptureStack);
    }

    reportException(exec, exception, cachedScript);
}

String retrieveErrorMessage(ExecState& state, VM& vm, JSValue exception, CatchScope& catchScope)
{
    if (auto* exceptionBase = toExceptionBase(exception))
        return exceptionBase->toString();

    // FIXME: <http://webkit.org/b/115087> Web Inspector: WebCore::reportException should not evaluate JavaScript handling exceptions
    // If this is a custom exception object, call toString on it to try and get a nice string representation for the exception.
    String errorMessage;
    if (auto* error = jsDynamicDowncast<ErrorInstance*>(exception))
        errorMessage = error->sanitizedToString(&state);
    else
        errorMessage = exception.toWTFString(&state);

    // We need to clear any new exception that may be thrown in the toString() call above.
    // reportException() is not supposed to be making new exceptions.
    catchScope.clearException();
    vm.clearLastException();
    return errorMessage;
}

void reportException(ExecState* exec, JSC::Exception* exception, CachedScript* cachedScript, ExceptionDetails* exceptionDetails)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    RELEASE_ASSERT(vm.currentThreadIsHoldingAPILock());
    if (isTerminatedExecutionException(exception))
        return;

    ErrorHandlingScope errorScope(exec->vm());

    RefPtr<ScriptCallStack> callStack(createScriptCallStackFromException(exec, exception, ScriptCallStack::maxCallStackSizeToCapture));
    scope.clearException();
    vm.clearLastException();

    JSDOMGlobalObject* globalObject = jsCast<JSDOMGlobalObject*>(exec->lexicalGlobalObject());
    if (JSDOMWindow* window = jsDynamicDowncast<JSDOMWindow*>(globalObject)) {
        if (!window->wrapped().isCurrentlyDisplayedInFrame())
            return;
    }

    int lineNumber = 0;
    int columnNumber = 0;
    String exceptionSourceURL;
    if (const ScriptCallFrame* callFrame = callStack->firstNonNativeCallFrame()) {
        lineNumber = callFrame->lineNumber();
        columnNumber = callFrame->columnNumber();
        exceptionSourceURL = callFrame->sourceURL();
    }

    String errorMessage = retrieveErrorMessage(*exec, vm, exception->value(), scope);
    ScriptExecutionContext* scriptExecutionContext = globalObject->scriptExecutionContext();
    scriptExecutionContext->reportException(errorMessage, lineNumber, columnNumber, exceptionSourceURL, exception, callStack->size() ? callStack : nullptr, cachedScript);

    if (exceptionDetails) {
        exceptionDetails->message = errorMessage;
        exceptionDetails->lineNumber = lineNumber;
        exceptionDetails->columnNumber = columnNumber;
        exceptionDetails->sourceURL = exceptionSourceURL;
    }
}

void reportCurrentException(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);
    auto* exception = scope.exception();
    scope.clearException();
    reportException(exec, exception);
}

static JSValue createDOMException(ExecState* exec, ExceptionCode ec, const String* message = nullptr)
{
    if (!ec || ec == ExistingExceptionError)
        return jsUndefined();

    // FIXME: Handle other WebIDL exception types.
    if (ec == TypeError) {
        if (!message || message->isEmpty())
            return createTypeError(exec);
        return createTypeError(exec, *message);
    }

    if (ec == RangeError) {
        if (!message || message->isEmpty())
            return createRangeError(exec, ASCIILiteral("Bad value"));
        return createRangeError(exec, *message);
    }

    if (ec == StackOverflowError)
        return createStackOverflowError(exec);

    // FIXME: All callers to createDOMException need to pass in the correct global object.
    // For now, we're going to assume the lexicalGlobalObject. Which is wrong in cases like this:
    // frames[0].document.createElement(null, null); // throws an exception which should have the subframe's prototypes.
    JSDOMGlobalObject* globalObject = deprecatedGlobalObjectForPrototype(exec);

    ExceptionCodeDescription description(ec);

    CString messageCString;
    if (message)
        messageCString = message->utf8();
    if (message && !message->isEmpty()) {
        // It is safe to do this because the char* contents of the CString are copied into a new WTF::String before the CString is destroyed.
        description.description = messageCString.data();
    }

    JSValue errorObject;
    switch (description.type) {
    case DOMCoreExceptionType:
#if ENABLE(INDEXED_DATABASE)
    case IDBDatabaseExceptionType:
#endif
        errorObject = toJS(exec, globalObject, DOMCoreException::create(description));
        break;
    case FileExceptionType:
        errorObject = toJS(exec, globalObject, FileException::create(description));
        break;
    case SQLExceptionType:
        errorObject = toJS(exec, globalObject, SQLException::create(description));
        break;
    case SVGExceptionType:
        errorObject = toJS(exec, globalObject, SVGException::create(description));
        break;
    case XPathExceptionType:
        errorObject = toJS(exec, globalObject, XPathException::create(description));
        break;
    }
    
    ASSERT(errorObject);
    addErrorInfo(exec, asObject(errorObject), true);
    return errorObject;
}

JSValue createDOMException(ExecState* exec, ExceptionCode ec, const String& message)
{
    return createDOMException(exec, ec, &message);
}

JSValue createDOMException(ExecState& state, Exception&& exception)
{
    return createDOMException(&state, exception.code(), exception.releaseMessage());
}

void propagateExceptionSlowPath(JSC::ExecState& state, JSC::ThrowScope& throwScope, Exception&& exception)
{
    ASSERT(!throwScope.exception());
    throwException(&state, throwScope, createDOMException(state, WTFMove(exception)));
}

bool hasIteratorMethod(JSC::ExecState& state, JSC::JSValue value)
{
    auto& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!value.isObject())
        return false;

    JSObject* object = JSC::asObject(value);
    CallData callData;
    CallType callType;
    JSValue applyMethod = object->getMethod(&state, callData, callType, vm.propertyNames->iteratorSymbol, ASCIILiteral("Symbol.iterator property should be callable"));
    RETURN_IF_EXCEPTION(scope, false);

    return !applyMethod.isUndefined();
}

bool BindingSecurity::shouldAllowAccessToFrame(ExecState& state, Frame& frame, String& message)
{
    if (BindingSecurity::shouldAllowAccessToFrame(&state, &frame, DoNotReportSecurityError))
        return true;
    message = frame.document()->domWindow()->crossDomainAccessErrorMessage(activeDOMWindow(&state));
    return false;
}

bool BindingSecurity::shouldAllowAccessToDOMWindow(ExecState& state, DOMWindow& globalObject, String& message)
{
    if (BindingSecurity::shouldAllowAccessToDOMWindow(&state, globalObject, DoNotReportSecurityError))
        return true;
    message = globalObject.crossDomainAccessErrorMessage(activeDOMWindow(&state));
    return false;
}

void printErrorMessageForFrame(Frame* frame, const String& message)
{
    if (!frame)
        return;
    frame->document()->domWindow()->printErrorMessage(message);
}

Structure* getCachedDOMStructure(JSDOMGlobalObject& globalObject, const ClassInfo* classInfo)
{
    JSDOMStructureMap& structures = globalObject.structures(NoLockingNecessary);
    return structures.get(classInfo).get();
}

Structure* cacheDOMStructure(JSDOMGlobalObject& globalObject, Structure* structure, const ClassInfo* classInfo)
{
    auto locker = lockDuringMarking(globalObject.vm().heap, globalObject.gcLock());
    JSDOMStructureMap& structures = globalObject.structures(locker);
    ASSERT(!structures.contains(classInfo));
    return structures.set(classInfo, WriteBarrier<Structure>(globalObject.vm(), &globalObject, structure)).iterator->value.get();
}

static const int32_t kMaxInt32 = 0x7fffffff;
static const int32_t kMinInt32 = -kMaxInt32 - 1;
static const uint32_t kMaxUInt32 = 0xffffffffU;
static const int64_t kJSMaxInteger = 0x20000000000000LL - 1; // 2^53 - 1, largest integer exactly representable in ECMAScript.

static String rangeErrorString(double value, double min, double max)
{
    return makeString("Value ", String::numberToStringECMAScript(value), " is outside the range [", String::numberToStringECMAScript(min), ", ", String::numberToStringECMAScript(max), "]");
}

static double enforceRange(ExecState& state, double x, double minimum, double maximum)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (std::isnan(x) || std::isinf(x)) {
        throwTypeError(&state, scope, rangeErrorString(x, minimum, maximum));
        return 0;
    }
    x = trunc(x);
    if (x < minimum || x > maximum) {
        throwTypeError(&state, scope, rangeErrorString(x, minimum, maximum));
        return 0;
    }
    return x;
}

template <typename T>
struct IntTypeLimits {
};

template <>
struct IntTypeLimits<int8_t> {
    static const int8_t minValue = -128;
    static const int8_t maxValue = 127;
    static const unsigned numberOfValues = 256; // 2^8
};

template <>
struct IntTypeLimits<uint8_t> {
    static const uint8_t maxValue = 255;
    static const unsigned numberOfValues = 256; // 2^8
};

template <>
struct IntTypeLimits<int16_t> {
    static const short minValue = -32768;
    static const short maxValue = 32767;
    static const unsigned numberOfValues = 65536; // 2^16
};

template <>
struct IntTypeLimits<uint16_t> {
    static const unsigned short maxValue = 65535;
    static const unsigned numberOfValues = 65536; // 2^16
};

template <typename T>
static inline T toSmallerInt(ExecState& state, JSValue value, IntegerConversionConfiguration configuration)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    static_assert(std::is_signed<T>::value && std::is_integral<T>::value, "Should only be used for signed integral types");

    typedef IntTypeLimits<T> LimitsTrait;
    // Fast path if the value is already a 32-bit signed integer in the right range.
    if (value.isInt32()) {
        int32_t d = value.asInt32();
        if (d >= LimitsTrait::minValue && d <= LimitsTrait::maxValue)
            return static_cast<T>(d);
        switch (configuration) {
        case IntegerConversionConfiguration::Normal:
            break;
        case IntegerConversionConfiguration::EnforceRange:
            throwTypeError(&state, scope);
            return 0;
        case IntegerConversionConfiguration::Clamp:
            return d < LimitsTrait::minValue ? LimitsTrait::minValue : LimitsTrait::maxValue;
        }
        d %= LimitsTrait::numberOfValues;
        return static_cast<T>(d > LimitsTrait::maxValue ? d - LimitsTrait::numberOfValues : d);
    }

    double x = value.toNumber(&state);
    RETURN_IF_EXCEPTION(scope, 0);

    switch (configuration) {
    case IntegerConversionConfiguration::Normal:
        break;
    case IntegerConversionConfiguration::EnforceRange:
        return enforceRange(state, x, LimitsTrait::minValue, LimitsTrait::maxValue);
    case IntegerConversionConfiguration::Clamp:
        return std::isnan(x) ? 0 : clampTo<T>(x);
    }

    if (std::isnan(x) || std::isinf(x) || !x)
        return 0;

    x = x < 0 ? -floor(fabs(x)) : floor(fabs(x));
    x = fmod(x, LimitsTrait::numberOfValues);

    return static_cast<T>(x > LimitsTrait::maxValue ? x - LimitsTrait::numberOfValues : x);
}

template <typename T>
static inline T toSmallerUInt(ExecState& state, JSValue value, IntegerConversionConfiguration configuration)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    static_assert(std::is_unsigned<T>::value && std::is_integral<T>::value, "Should only be used for unsigned integral types");

    typedef IntTypeLimits<T> LimitsTrait;
    // Fast path if the value is already a 32-bit unsigned integer in the right range.
    if (value.isUInt32()) {
        uint32_t d = value.asUInt32();
        if (d <= LimitsTrait::maxValue)
            return static_cast<T>(d);
        switch (configuration) {
        case IntegerConversionConfiguration::Normal:
            return static_cast<T>(d);
        case IntegerConversionConfiguration::EnforceRange:
            throwTypeError(&state, scope);
            return 0;
        case IntegerConversionConfiguration::Clamp:
            return LimitsTrait::maxValue;
        }
    }

    double x = value.toNumber(&state);
    RETURN_IF_EXCEPTION(scope, 0);

    switch (configuration) {
    case IntegerConversionConfiguration::Normal:
        break;
    case IntegerConversionConfiguration::EnforceRange:
        return enforceRange(state, x, 0, LimitsTrait::maxValue);
    case IntegerConversionConfiguration::Clamp:
        return std::isnan(x) ? 0 : clampTo<T>(x);
    }

    if (std::isnan(x) || std::isinf(x) || !x)
        return 0;

    x = x < 0 ? -floor(fabs(x)) : floor(fabs(x));
    return static_cast<T>(fmod(x, LimitsTrait::numberOfValues));
}

int8_t toInt8EnforceRange(JSC::ExecState& state, JSValue value)
{
    return toSmallerInt<int8_t>(state, value, IntegerConversionConfiguration::EnforceRange);
}

uint8_t toUInt8EnforceRange(JSC::ExecState& state, JSValue value)
{
    return toSmallerUInt<uint8_t>(state, value, IntegerConversionConfiguration::EnforceRange);
}

int8_t toInt8Clamp(JSC::ExecState& state, JSValue value)
{
    return toSmallerInt<int8_t>(state, value, IntegerConversionConfiguration::Clamp);
}

uint8_t toUInt8Clamp(JSC::ExecState& state, JSValue value)
{
    return toSmallerUInt<uint8_t>(state, value, IntegerConversionConfiguration::Clamp);
}

// http://www.w3.org/TR/WebIDL/#es-byte
int8_t toInt8(ExecState& state, JSValue value)
{
    return toSmallerInt<int8_t>(state, value, IntegerConversionConfiguration::Normal);
}

// http://www.w3.org/TR/WebIDL/#es-octet
uint8_t toUInt8(ExecState& state, JSValue value)
{
    return toSmallerUInt<uint8_t>(state, value, IntegerConversionConfiguration::Normal);
}

int16_t toInt16EnforceRange(ExecState& state, JSValue value)
{
    return toSmallerInt<int16_t>(state, value, IntegerConversionConfiguration::EnforceRange);
}

uint16_t toUInt16EnforceRange(ExecState& state, JSValue value)
{
    return toSmallerUInt<uint16_t>(state, value, IntegerConversionConfiguration::EnforceRange);
}

int16_t toInt16Clamp(ExecState& state, JSValue value)
{
    return toSmallerInt<int16_t>(state, value, IntegerConversionConfiguration::Clamp);
}

uint16_t toUInt16Clamp(ExecState& state, JSValue value)
{
    return toSmallerUInt<uint16_t>(state, value, IntegerConversionConfiguration::Clamp);
}

// http://www.w3.org/TR/WebIDL/#es-short
int16_t toInt16(ExecState& state, JSValue value)
{
    return toSmallerInt<int16_t>(state, value, IntegerConversionConfiguration::Normal);
}

// http://www.w3.org/TR/WebIDL/#es-unsigned-short
uint16_t toUInt16(ExecState& state, JSValue value)
{
    return toSmallerUInt<uint16_t>(state, value, IntegerConversionConfiguration::Normal);
}

// http://www.w3.org/TR/WebIDL/#es-long
int32_t toInt32EnforceRange(ExecState& state, JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (value.isInt32())
        return value.asInt32();

    double x = value.toNumber(&state);
    RETURN_IF_EXCEPTION(scope, 0);
    return enforceRange(state, x, kMinInt32, kMaxInt32);
}

int32_t toInt32Clamp(ExecState& state, JSValue value)
{
    if (value.isInt32())
        return value.asInt32();

    double x = value.toNumber(&state);
    return std::isnan(x) ? 0 : clampTo<int32_t>(x);
}

uint32_t toUInt32Clamp(ExecState& state, JSValue value)
{
    if (value.isUInt32())
        return value.asUInt32();

    double x = value.toNumber(&state);
    return std::isnan(x) ? 0 : clampTo<uint32_t>(x);
}

// http://www.w3.org/TR/WebIDL/#es-unsigned-long
uint32_t toUInt32EnforceRange(ExecState& state, JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (value.isUInt32())
        return value.asUInt32();

    double x = value.toNumber(&state);
    RETURN_IF_EXCEPTION(scope, 0);
    return enforceRange(state, x, 0, kMaxUInt32);
}

int64_t toInt64EnforceRange(ExecState& state, JSC::JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double x = value.toNumber(&state);
    RETURN_IF_EXCEPTION(scope, 0);
    return enforceRange(state, x, -kJSMaxInteger, kJSMaxInteger);
}

uint64_t toUInt64EnforceRange(ExecState& state, JSC::JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double x = value.toNumber(&state);
    RETURN_IF_EXCEPTION(scope, 0);
    return enforceRange(state, x, 0, kJSMaxInteger);
}

int64_t toInt64Clamp(ExecState& state, JSC::JSValue value)
{
    double x = value.toNumber(&state);
    return std::isnan(x) ? 0 : static_cast<int64_t>(std::min<double>(std::max<double>(x, -kJSMaxInteger), kJSMaxInteger));
}

uint64_t toUInt64Clamp(ExecState& state, JSC::JSValue value)
{
    double x = value.toNumber(&state);
    return std::isnan(x) ? 0 : static_cast<uint64_t>(std::min<double>(std::max<double>(x, 0), kJSMaxInteger));
}

// http://www.w3.org/TR/WebIDL/#es-long-long
int64_t toInt64(ExecState& state, JSValue value)
{
    double x = value.toNumber(&state);

    // Map NaNs and +/-Infinity to 0; convert finite values modulo 2^64.
    unsigned long long n;
    doubleToInteger(x, n);
    return n;
}

// http://www.w3.org/TR/WebIDL/#es-unsigned-long-long
uint64_t toUInt64(ExecState& state, JSValue value)
{
    double x = value.toNumber(&state);

    // Map NaNs and +/-Infinity to 0; convert finite values modulo 2^64.
    unsigned long long n;
    doubleToInteger(x, n);
    return n;
}

class GetCallerGlobalObjectFunctor {
public:
    GetCallerGlobalObjectFunctor() = default;

    StackVisitor::Status operator()(StackVisitor& visitor) const
    {
        if (!m_hasSkippedFirstFrame) {
            m_hasSkippedFirstFrame = true;
            return StackVisitor::Continue;
        }

        if (auto* codeBlock = visitor->codeBlock())
            m_globalObject = codeBlock->globalObject();
        else {
            ASSERT(visitor->callee());
            // FIXME: Callee is not an object if the caller is Web Assembly.
            // Figure out what to do here. We can probably get the global object
            // from the top-most Wasm Instance. https://bugs.webkit.org/show_bug.cgi?id=165721
            if (visitor->callee()->isObject())
                m_globalObject = jsCast<JSObject*>(visitor->callee())->globalObject();
        }
        return StackVisitor::Done;
    }

    JSGlobalObject* globalObject() const { return m_globalObject; }

private:
    mutable bool m_hasSkippedFirstFrame { false };
    mutable JSGlobalObject* m_globalObject { nullptr };
};

DOMWindow& callerDOMWindow(ExecState* exec)
{
    GetCallerGlobalObjectFunctor iter;
    exec->iterate(iter);
    return iter.globalObject() ? asJSDOMWindow(iter.globalObject())->wrapped() : firstDOMWindow(exec);
}

DOMWindow& activeDOMWindow(ExecState* exec)
{
    return asJSDOMWindow(exec->lexicalGlobalObject())->wrapped();
}

DOMWindow& firstDOMWindow(ExecState* exec)
{
    return asJSDOMWindow(exec->vmEntryGlobalObject())->wrapped();
}

static inline bool canAccessDocument(JSC::ExecState* state, Document* targetDocument, SecurityReportingOption reportingOption)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!targetDocument)
        return false;

    DOMWindow& active = activeDOMWindow(state);

    if (active.document()->securityOrigin().canAccess(targetDocument->securityOrigin()))
        return true;

    switch (reportingOption) {
    case ThrowSecurityError:
        throwSecurityError(*state, scope, targetDocument->domWindow()->crossDomainAccessErrorMessage(active));
        break;
    case LogSecurityError:
        printErrorMessageForFrame(targetDocument->frame(), targetDocument->domWindow()->crossDomainAccessErrorMessage(active));
        break;
    case DoNotReportSecurityError:
        break;
    }

    return false;
}

bool BindingSecurity::shouldAllowAccessToDOMWindow(JSC::ExecState* state, DOMWindow& target, SecurityReportingOption reportingOption)
{
    return canAccessDocument(state, target.document(), reportingOption);
}

bool BindingSecurity::shouldAllowAccessToFrame(JSC::ExecState* state, Frame* target, SecurityReportingOption reportingOption)
{
    return target && canAccessDocument(state, target->document(), reportingOption);
}

bool BindingSecurity::shouldAllowAccessToNode(JSC::ExecState& state, Node* target)
{
    return !target || canAccessDocument(&state, &target->document(), LogSecurityError);
}
    
static EncodedJSValue throwTypeError(JSC::ExecState& state, JSC::ThrowScope& scope, const String& errorMessage)
{
    return throwVMTypeError(&state, scope, errorMessage);
}

static void appendArgumentMustBe(StringBuilder& builder, unsigned argumentIndex, const char* argumentName, const char* interfaceName, const char* functionName)
{
    builder.appendLiteral("Argument ");
    builder.appendNumber(argumentIndex + 1);
    builder.appendLiteral(" ('");
    builder.append(argumentName);
    builder.appendLiteral("') to ");
    if (!functionName) {
        builder.appendLiteral("the ");
        builder.append(interfaceName);
        builder.appendLiteral(" constructor");
    } else {
        builder.append(interfaceName);
        builder.append('.');
        builder.append(functionName);
    }
    builder.appendLiteral(" must be ");
}

JSC::EncodedJSValue reportDeprecatedGetterError(JSC::ExecState& state, const char* interfaceName, const char* attributeName)
{
    auto& context = *jsCast<JSDOMGlobalObject*>(state.lexicalGlobalObject())->scriptExecutionContext();
    context.addConsoleMessage(MessageSource::JS, MessageLevel::Error, makeString("Deprecated attempt to access property '", attributeName, "' on a non-", interfaceName, " object."));
    return JSValue::encode(jsUndefined());
}
    
void reportDeprecatedSetterError(JSC::ExecState& state, const char* interfaceName, const char* attributeName)
{
    auto& context = *jsCast<JSDOMGlobalObject*>(state.lexicalGlobalObject())->scriptExecutionContext();
    context.addConsoleMessage(MessageSource::JS, MessageLevel::Error, makeString("Deprecated attempt to set property '", attributeName, "' on a non-", interfaceName, " object."));
}

void throwNotSupportedError(JSC::ExecState& state, JSC::ThrowScope& scope)
{
    ASSERT(!scope.exception());
    throwException(&state, scope, createDOMException(&state, NOT_SUPPORTED_ERR));
}

void throwNotSupportedError(JSC::ExecState& state, JSC::ThrowScope& scope, const char* message)
{
    ASSERT(!scope.exception());
    String messageString(message);
    throwException(&state, scope, createDOMException(&state, NOT_SUPPORTED_ERR, &messageString));
}

void throwInvalidStateError(JSC::ExecState& state, JSC::ThrowScope& scope, const char* message)
{
    ASSERT(!scope.exception());
    String messageString(message);
    throwException(&state, scope, createDOMException(&state, INVALID_STATE_ERR, &messageString));
}

void throwSecurityError(JSC::ExecState& state, JSC::ThrowScope& scope, const String& message)
{
    ASSERT(!scope.exception());
    throwException(&state, scope, createDOMException(&state, SECURITY_ERR, message));
}

JSC::EncodedJSValue throwArgumentMustBeEnumError(JSC::ExecState& state, JSC::ThrowScope& scope, unsigned argumentIndex, const char* argumentName, const char* functionInterfaceName, const char* functionName, const char* expectedValues)
{
    StringBuilder builder;
    appendArgumentMustBe(builder, argumentIndex, argumentName, functionInterfaceName, functionName);
    builder.appendLiteral("one of: ");
    builder.append(expectedValues);
    return throwVMTypeError(&state, scope, builder.toString());
}

JSC::EncodedJSValue throwArgumentMustBeFunctionError(JSC::ExecState& state, JSC::ThrowScope& scope, unsigned argumentIndex, const char* argumentName, const char* interfaceName, const char* functionName)
{
    StringBuilder builder;
    appendArgumentMustBe(builder, argumentIndex, argumentName, interfaceName, functionName);
    builder.appendLiteral("a function");
    return throwVMTypeError(&state, scope, builder.toString());
}

JSC::EncodedJSValue throwArgumentTypeError(JSC::ExecState& state, JSC::ThrowScope& scope, unsigned argumentIndex, const char* argumentName, const char* functionInterfaceName, const char* functionName, const char* expectedType)
{
    StringBuilder builder;
    appendArgumentMustBe(builder, argumentIndex, argumentName, functionInterfaceName, functionName);
    builder.appendLiteral("an instance of ");
    builder.append(expectedType);
    return throwVMTypeError(&state, scope, builder.toString());
}

void throwArrayElementTypeError(JSC::ExecState& state, JSC::ThrowScope& scope)
{
    throwTypeError(state, scope, ASCIILiteral("Invalid Array element type"));
}

void throwAttributeTypeError(JSC::ExecState& state, JSC::ThrowScope& scope, const char* interfaceName, const char* attributeName, const char* expectedType)
{
    throwTypeError(state, scope, makeString("The ", interfaceName, '.', attributeName, " attribute must be an instance of ", expectedType));
}

JSC::EncodedJSValue throwRequiredMemberTypeError(JSC::ExecState& state, JSC::ThrowScope& scope, const char* memberName, const char* dictionaryName, const char* expectedType)
{
    StringBuilder builder;
    builder.appendLiteral("Member ");
    builder.append(dictionaryName);
    builder.append('.');
    builder.append(memberName);
    builder.appendLiteral(" is required and must be an instance of ");
    builder.append(expectedType);
    return throwVMTypeError(&state, scope, builder.toString());
}

JSC::EncodedJSValue throwConstructorScriptExecutionContextUnavailableError(JSC::ExecState& state, JSC::ThrowScope& scope, const char* interfaceName)
{
    return throwVMError(&state, scope, createReferenceError(&state, makeString(interfaceName, " constructor associated execution context is unavailable")));
}

void throwSequenceTypeError(JSC::ExecState& state, JSC::ThrowScope& scope)
{
    throwTypeError(state, scope, ASCIILiteral("Value is not a sequence"));
}

void throwNonFiniteTypeError(ExecState& state, JSC::ThrowScope& scope)
{
    throwTypeError(&state, scope, ASCIILiteral("The provided value is non-finite"));
}

String makeGetterTypeErrorMessage(const char* interfaceName, const char* attributeName)
{
    return makeString("The ", interfaceName, '.', attributeName, " getter can only be used on instances of ", interfaceName);
}

JSC::EncodedJSValue throwGetterTypeError(JSC::ExecState& state, JSC::ThrowScope& scope, const char* interfaceName, const char* attributeName)
{
    return throwVMTypeError(&state, scope, makeGetterTypeErrorMessage(interfaceName, attributeName));
}

JSC::EncodedJSValue rejectPromiseWithGetterTypeError(JSC::ExecState& state, const char* interfaceName, const char* attributeName)
{
    return createRejectedPromiseWithTypeError(state, makeGetterTypeErrorMessage(interfaceName, attributeName));
}

bool throwSetterTypeError(JSC::ExecState& state, JSC::ThrowScope& scope, const char* interfaceName, const char* attributeName)
{
    throwTypeError(state, scope, makeString("The ", interfaceName, '.', attributeName, " setter can only be used on instances of ", interfaceName));
    return false;
}

String makeThisTypeErrorMessage(const char* interfaceName, const char* functionName)
{
    return makeString("Can only call ", interfaceName, '.', functionName, " on instances of ", interfaceName);
}

EncodedJSValue throwThisTypeError(JSC::ExecState& state, JSC::ThrowScope& scope, const char* interfaceName, const char* functionName)
{
    return throwTypeError(state, scope, makeThisTypeErrorMessage(interfaceName, functionName));
}

JSC::EncodedJSValue rejectPromiseWithThisTypeError(DeferredPromise& promise, const char* interfaceName, const char* methodName)
{
    promise.reject(TypeError, makeThisTypeErrorMessage(interfaceName, methodName));
    return JSValue::encode(jsUndefined());
}

JSC::EncodedJSValue rejectPromiseWithThisTypeError(JSC::ExecState& state, const char* interfaceName, const char* methodName)
{
    return createRejectedPromiseWithTypeError(state, makeThisTypeErrorMessage(interfaceName, methodName));
}

void callFunctionWithCurrentArguments(JSC::ExecState& state, JSC::JSObject& thisObject, JSC::JSFunction& function)
{
    JSC::CallData callData;
    JSC::CallType callType = JSC::getCallData(&function, callData);
    ASSERT(callType != CallType::None);

    JSC::MarkedArgumentBuffer arguments;
    for (unsigned i = 0; i < state.argumentCount(); ++i)
        arguments.append(state.uncheckedArgument(i));
    JSC::call(&state, &function, callType, callData, &thisObject, arguments);
}

void DOMConstructorJSBuiltinObject::visitChildren(JSC::JSCell* cell, JSC::SlotVisitor& visitor)
{
    auto* thisObject = jsCast<DOMConstructorJSBuiltinObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_initializeFunction);
}

static EncodedJSValue JSC_HOST_CALL callThrowTypeError(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    throwTypeError(exec, scope, ASCIILiteral("Constructor requires 'new' operator"));
    return JSValue::encode(jsNull());
}

CallType DOMConstructorObject::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callThrowTypeError;
    return CallType::Host;
}

void throwDOMSyntaxError(JSC::ExecState& state, JSC::ThrowScope& scope)
{
    ASSERT(!scope.exception());
    throwException(&state, scope, createDOMException(&state, SYNTAX_ERR));
}

void throwDataCloneError(JSC::ExecState& state, JSC::ThrowScope& scope)
{
    ASSERT(!scope.exception());
    throwException(&state, scope, createDOMException(&state, DATA_CLONE_ERR));
}

void throwIndexSizeError(JSC::ExecState& state, JSC::ThrowScope& scope)
{
    ASSERT(!scope.exception());
    throwException(&state, scope, createDOMException(&state, INDEX_SIZE_ERR));
}

void throwTypeMismatchError(JSC::ExecState& state, JSC::ThrowScope& scope)
{
    ASSERT(!scope.exception());
    throwException(&state, scope, createDOMException(&state, TYPE_MISMATCH_ERR));
}

} // namespace WebCore
