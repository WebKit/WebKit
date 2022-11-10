/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2021 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "ErrorInstance.h"

#include "CodeBlock.h"
#include "InlineCallFrame.h"
#include "IntegrityInlines.h"
#include "Interpreter.h"
#include "JSCInlines.h"
#include "ParseInt.h"
#include "StackFrame.h"

namespace JSC {

const ClassInfo ErrorInstance::s_info = { "Error"_s, &JSNonFinalObject::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ErrorInstance) };

ErrorInstance::ErrorInstance(VM& vm, Structure* structure, ErrorType errorType)
    : Base(vm, structure)
    , m_errorType(errorType)
    , m_stackOverflowError(false)
    , m_outOfMemoryError(false)
    , m_errorInfoMaterialized(false)
    , m_nativeGetterTypeError(false)
#if ENABLE(WEBASSEMBLY)
    , m_catchableFromWasm(true)
#endif // ENABLE(WEBASSEMBLY)
{
}

ErrorInstance* ErrorInstance::create(JSGlobalObject* globalObject, Structure* structure, JSValue message, JSValue options, SourceAppender appender, RuntimeType type, ErrorType errorType, bool useCurrentFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String messageString = message.isUndefined() ? String() : message.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    JSValue cause;
    if (options.isObject()) {
        // Since `throw undefined;` is valid, we need to distinguish the case where `cause` is an explicit undefined.
        cause = asObject(options)->getIfPropertyExists(globalObject, vm.propertyNames->cause);
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    return create(globalObject, vm, structure, messageString, cause, appender, type, errorType, useCurrentFrame);
}

static String appendSourceToErrorMessage(CodeBlock* codeBlock, ErrorInstance* exception, BytecodeIndex bytecodeIndex, const String& message)
{
    ErrorInstance::SourceAppender appender = exception->sourceAppender();
    exception->clearSourceAppender();
    RuntimeType type = exception->runtimeTypeForCause();
    exception->clearRuntimeTypeForCause();

    if (!codeBlock->hasExpressionInfo() || message.isNull())
        return message;
    
    int startOffset = 0;
    int endOffset = 0;
    int divotPoint = 0;
    unsigned line = 0;
    unsigned column = 0;

    codeBlock->expressionRangeForBytecodeIndex(bytecodeIndex, divotPoint, startOffset, endOffset, line, column);
    
    int expressionStart = divotPoint - startOffset;
    int expressionStop = divotPoint + endOffset;

    StringView sourceString = codeBlock->source().provider()->source();
    if (!expressionStop || expressionStart > static_cast<int>(sourceString.length()))
        return message;
    
    if (expressionStart < expressionStop)
        return appender(message, codeBlock->source().provider()->getRange(expressionStart, expressionStop), type, ErrorInstance::FoundExactSource);

    // No range information, so give a few characters of context.
    int dataLength = sourceString.length();
    int start = expressionStart;
    int stop = expressionStart;
    // Get up to 20 characters of context to the left and right of the divot, clamping to the line.
    // Then strip whitespace.
    while (start > 0 && (expressionStart - start < 20) && sourceString[start - 1] != '\n')
        start--;
    while (start < (expressionStart - 1) && isStrWhiteSpace(sourceString[start]))
        start++;
    while (stop < dataLength && (stop - expressionStart < 20) && sourceString[stop] != '\n')
        stop++;
    while (stop > expressionStart && isStrWhiteSpace(sourceString[stop - 1]))
        stop--;
    return appender(message, codeBlock->source().provider()->getRange(start, stop), type, ErrorInstance::FoundApproximateSource);
}

void ErrorInstance::finishCreation(VM& vm, JSGlobalObject* globalObject, const String& message, JSValue cause, SourceAppender appender, RuntimeType type, bool useCurrentFrame)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    m_sourceAppender = appender;
    m_runtimeTypeForCause = type;

    std::unique_ptr<Vector<StackFrame>> stackTrace = getStackTrace(globalObject, vm, this, useCurrentFrame);
    {
        Locker locker { cellLock() };
        m_stackTrace = WTFMove(stackTrace);
    }
    vm.writeBarrier(this);

    String messageWithSource = message;
    if (m_stackTrace && !m_stackTrace->isEmpty() && hasSourceAppender()) {
        auto [codeBlock, bytecodeIndex] = getBytecodeIndex(vm, vm.topCallFrame);
        if (codeBlock)
            messageWithSource = appendSourceToErrorMessage(codeBlock, this, bytecodeIndex, message);
    }

    if (!messageWithSource.isNull())
        putDirect(vm, vm.propertyNames->message, jsString(vm, WTFMove(messageWithSource)), static_cast<unsigned>(PropertyAttribute::DontEnum));

    if (!cause.isEmpty())
        putDirect(vm, vm.propertyNames->cause, cause, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

// Based on ErrorPrototype's errorProtoFuncToString(), but is modified to
// have no observable side effects to the user (i.e. does not call proxies,
// and getters).
String ErrorInstance::sanitizedMessageString(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    Integrity::auditStructureID(structureID());

    JSValue messageValue;
    auto messagePropertName = vm.propertyNames->message;
    PropertySlot messageSlot(this, PropertySlot::InternalMethodType::VMInquiry, &vm);
    if (JSObject::getOwnPropertySlot(this, globalObject, messagePropertName, messageSlot) && messageSlot.isValue())
        messageValue = messageSlot.getValue(globalObject, messagePropertName);
    RETURN_IF_EXCEPTION(scope, { });

    if (!messageValue || !messageValue.isPrimitive())
        return { };

    RELEASE_AND_RETURN(scope, messageValue.toWTFString(globalObject));
}

String ErrorInstance::sanitizedNameString(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    Integrity::auditStructureID(structureID());

    JSValue nameValue;
    auto namePropertName = vm.propertyNames->name;
    PropertySlot nameSlot(this, PropertySlot::InternalMethodType::VMInquiry, &vm);

    JSValue currentObj = this;
    unsigned prototypeDepth = 0;

    // We only check the current object and its prototype (2 levels) because normal
    // Error objects may have a name property, and if not, its prototype should have
    // a name property for the type of error e.g. "SyntaxError".
    while (currentObj.isCell() && prototypeDepth++ < 2) {
        JSObject* obj = jsCast<JSObject*>(currentObj);
        if (JSObject::getOwnPropertySlot(obj, globalObject, namePropertName, nameSlot) && nameSlot.isValue()) {
            nameValue = nameSlot.getValue(globalObject, namePropertName);
            break;
        }
        currentObj = obj->getPrototypeDirect();
    }
    RETURN_IF_EXCEPTION(scope, { });

    if (!nameValue || !nameValue.isPrimitive())
        return "Error"_s;
    RELEASE_AND_RETURN(scope, nameValue.toWTFString(globalObject));
}

String ErrorInstance::sanitizedToString(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    Integrity::auditStructureID(structureID());

    String nameString = sanitizedNameString(globalObject);
    RETURN_IF_EXCEPTION(scope, String());

    String messageString = sanitizedMessageString(globalObject);
    RETURN_IF_EXCEPTION(scope, String());

    return makeString(nameString, nameString.isEmpty() || messageString.isEmpty() ? "" : ": ", messageString);
}

void ErrorInstance::finalizeUnconditionally(VM& vm)
{
    if (!m_stackTrace)
        return;

    // We don't want to keep our stack traces alive forever if the user doesn't access the stack trace.
    // If we did, we might end up keeping functions (and their global objects) alive that happened to
    // get caught in a trace.
    for (const auto& frame : *m_stackTrace.get()) {
        if (!frame.isMarked(vm)) {
            computeErrorInfo(vm);
            return;
        }
    }
}

void ErrorInstance::computeErrorInfo(VM& vm)
{
    ASSERT(!m_errorInfoMaterialized);

    if (m_stackTrace && !m_stackTrace->isEmpty()) {
        getLineColumnAndSource(vm, m_stackTrace.get(), m_line, m_column, m_sourceURL);
        m_stackString = Interpreter::stackTraceAsString(vm, *m_stackTrace.get());
        m_stackTrace = nullptr;
    }
}

bool ErrorInstance::materializeErrorInfoIfNeeded(VM& vm)
{
    if (m_errorInfoMaterialized)
        return false;

    computeErrorInfo(vm);

    if (!m_stackString.isNull()) {
        auto attributes = static_cast<unsigned>(PropertyAttribute::DontEnum);

        putDirect(vm, vm.propertyNames->line, jsNumber(m_line), attributes);
        putDirect(vm, vm.propertyNames->column, jsNumber(m_column), attributes);
        if (!m_sourceURL.isEmpty())
            putDirect(vm, vm.propertyNames->sourceURL, jsString(vm, WTFMove(m_sourceURL)), attributes);

        putDirect(vm, vm.propertyNames->stack, jsString(vm, WTFMove(m_stackString)), attributes);
    }

    m_errorInfoMaterialized = true;
    return true;
}

bool ErrorInstance::materializeErrorInfoIfNeeded(VM& vm, PropertyName propertyName)
{
    if (propertyName == vm.propertyNames->line
        || propertyName == vm.propertyNames->column
        || propertyName == vm.propertyNames->sourceURL
        || propertyName == vm.propertyNames->stack)
        return materializeErrorInfoIfNeeded(vm);
    return false;
}

bool ErrorInstance::getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = globalObject->vm();
    ErrorInstance* thisObject = jsCast<ErrorInstance*>(object);
    thisObject->materializeErrorInfoIfNeeded(vm, propertyName);
    return Base::getOwnPropertySlot(thisObject, globalObject, propertyName, slot);
}

void ErrorInstance::getOwnSpecialPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArray&, DontEnumPropertiesMode mode)
{
    VM& vm = globalObject->vm();
    ErrorInstance* thisObject = jsCast<ErrorInstance*>(object);
    if (mode == DontEnumPropertiesMode::Include)
        thisObject->materializeErrorInfoIfNeeded(vm);
}

bool ErrorInstance::defineOwnProperty(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    VM& vm = globalObject->vm();
    ErrorInstance* thisObject = jsCast<ErrorInstance*>(object);
    thisObject->materializeErrorInfoIfNeeded(vm, propertyName);
    return Base::defineOwnProperty(thisObject, globalObject, propertyName, descriptor, shouldThrow);
}

bool ErrorInstance::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = globalObject->vm();
    ErrorInstance* thisObject = jsCast<ErrorInstance*>(cell);
    bool materializedProperties = thisObject->materializeErrorInfoIfNeeded(vm, propertyName);
    if (materializedProperties)
        slot.disableCaching();
    return Base::put(thisObject, globalObject, propertyName, value, slot);
}

bool ErrorInstance::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    VM& vm = globalObject->vm();
    ErrorInstance* thisObject = jsCast<ErrorInstance*>(cell);
    bool materializedProperties = thisObject->materializeErrorInfoIfNeeded(vm, propertyName);
    if (materializedProperties)
        slot.disableCaching();
    return Base::deleteProperty(thisObject, globalObject, propertyName, slot);
}

} // namespace JSC
