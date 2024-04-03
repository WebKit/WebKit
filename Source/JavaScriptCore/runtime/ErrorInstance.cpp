/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2024 Apple Inc. All rights reserved.
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

ErrorInstance* ErrorInstance::create(JSGlobalObject* globalObject, String&& message, ErrorType errorType, LineColumn lineColumn, String&& sourceURL, String&& stackString)
{
    VM& vm = globalObject->vm();
    Structure* structure = globalObject->errorStructure(errorType);
    ErrorInstance* instance = new (NotNull, allocateCell<ErrorInstance>(vm)) ErrorInstance(vm, structure, errorType);
    instance->finishCreation(vm, WTFMove(message), lineColumn, WTFMove(sourceURL), WTFMove(stackString));
    return instance;
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

    return create(vm, structure, messageString, cause, appender, type, errorType, useCurrentFrame);
}

String appendSourceToErrorMessage(CodeBlock* codeBlock, BytecodeIndex bytecodeIndex, const String& message, RuntimeType type, ErrorInstance::SourceAppender appender)
{
    if (!codeBlock->hasExpressionInfo() || message.isNull())
        return message;

    auto info = codeBlock->expressionInfoForBytecodeIndex(bytecodeIndex);
    int expressionStart = info.divot - info.startOffset;
    int expressionStop = info.divot + info.endOffset;

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

void ErrorInstance::captureStackTrace(VM& vm, JSGlobalObject* globalObject, size_t framesToSkip, bool append)
{
    {
        Locker locker { cellLock() };

        size_t limit = globalObject->stackTraceLimit().value();
        std::unique_ptr<Vector<StackFrame>> stackTrace = makeUnique<Vector<StackFrame>>();
        vm.interpreter.getStackTrace(this, *stackTrace, framesToSkip, limit);

        if (!m_stackTrace || !append) {
            m_stackTrace = WTFMove(stackTrace);
            vm.writeBarrier(this);
            return;
        }

        if (m_stackTrace) {
            size_t remaining = limit - std::min(stackTrace->size(), limit);
            remaining = std::min(remaining, m_stackTrace->size());
            if (remaining > 0) {
                ASSERT(m_stackTrace->size() >= remaining);
                stackTrace->append(std::span { m_stackTrace->data(), remaining } );
            }
        }

        m_stackTrace = WTFMove(stackTrace);
    }
    vm.writeBarrier(this);
}

void ErrorInstance::finishCreation(VM& vm, const String& message, JSValue cause, SourceAppender appender, RuntimeType type, bool useCurrentFrame)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    m_sourceAppender = appender;
    m_runtimeTypeForCause = type;

    std::unique_ptr<Vector<StackFrame>> stackTrace = getStackTrace(vm, this, useCurrentFrame);
    {
        Locker locker { cellLock() };
        m_stackTrace = WTFMove(stackTrace);
    }
    vm.writeBarrier(this);

    String messageWithSource = message;

    if (m_stackTrace && !m_stackTrace->isEmpty() && hasSourceAppender()) {
        auto [codeBlock, bytecodeIndex] = getBytecodeIndex(vm, vm.topCallFrame);
        if (codeBlock) {
            ErrorInstance::SourceAppender appender = sourceAppender();
            clearSourceAppender();
            RuntimeType type = runtimeTypeForCause();
            clearRuntimeTypeForCause();
            messageWithSource = appendSourceToErrorMessage(codeBlock, bytecodeIndex, message, type, appender);
        }
    }

    if (!messageWithSource.isNull()) {
        putDirect(vm, vm.propertyNames->message, jsString(vm, WTFMove(messageWithSource)), static_cast<unsigned>(PropertyAttribute::DontEnum));
    }

    if (!cause.isEmpty())
        putDirect(vm, vm.propertyNames->cause, cause, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

void ErrorInstance::finishCreation(VM& vm, const String& message, JSValue cause, JSCell* owner, CallLinkInfo* callLinkInfo)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    std::unique_ptr<Vector<StackFrame>> stackTrace = getStackTrace(vm, this, /* useCurrentFrame */ true, owner, callLinkInfo);
    {
        Locker locker { cellLock() };
        m_stackTrace = WTFMove(stackTrace);
    }
    vm.writeBarrier(this);
    if (!message.isNull())
        putDirect(vm, vm.propertyNames->message, jsString(vm, message), static_cast<unsigned>(PropertyAttribute::DontEnum));

    if (!cause.isEmpty())
        putDirect(vm, vm.propertyNames->cause, cause, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

void ErrorInstance::finishCreation(VM& vm, String&& message, LineColumn lineColumn, String&& sourceURL, String&& stackString)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    m_lineColumn = lineColumn;
    m_sourceURL = WTFMove(sourceURL);
    m_stackString = WTFMove(stackString);
    if (!message.isNull())
        putDirect(vm, vm.propertyNames->message, jsString(vm, WTFMove(message)), static_cast<unsigned>(PropertyAttribute::DontEnum));
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
    RETURN_IF_EXCEPTION(scope, {});

    if (!messageValue || !messageValue.isPrimitive())
        return {};

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
    RETURN_IF_EXCEPTION(scope, {});

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

    return makeString(nameString, nameString.isEmpty() || messageString.isEmpty() ? ""_s : ": "_s, messageString);
}

void ErrorInstance::finalizeUnconditionally(VM& vm, CollectionScope)
{
    if (!m_stackTrace)
        return;

    // We don't want to keep our stack traces alive forever if the user doesn't access the stack trace.
    // If we did, we might end up keeping functions (and their global objects) alive that happened to
    // get caught in a trace.
    for (const auto& frame : *m_stackTrace.get()) {
        if (!frame.isMarked(vm)) {
            computeErrorInfo(vm, false);
            return;
        }
    }
}

void ErrorInstance::computeErrorInfo(VM& vm, bool allocationAllowed)
{
    ASSERT(!m_errorInfoMaterialized);
    // Here we use DeferGCForAWhile instead of DeferGC since GC's Heap::runEndPhase can trigger this function. In
    // that case, DeferGC's destructor might trigger another GC cycle which is unexpected.
    DeferGCForAWhile deferGC(vm);

    if (m_stackTrace && !m_stackTrace->isEmpty()) {
        auto& fn = vm.onComputeErrorInfo();
        if (fn && allocationAllowed) {
            // This function may call `globalObject` or potentially even execute arbitrary JS code.
            // We cannot gurantee the lifetime of this stack trace to continue to be valid.
            // We have to move it out of the ErrorInstance.
            WTF::Vector<StackFrame> stackTrace = WTFMove(*m_stackTrace.get());
            m_stackString = fn(vm, stackTrace, m_lineColumn.line, m_lineColumn.column, m_sourceURL, allocationAllowed ? this : nullptr);
        } else if (fn && !allocationAllowed) {
            m_stackString = fn(vm, *m_stackTrace.get(), m_lineColumn.line, m_lineColumn.column, m_sourceURL, nullptr);
        } else {
            getLineColumnAndSource(vm, m_stackTrace.get(), m_lineColumn, m_sourceURL);
            m_stackString = Interpreter::stackTraceAsString(vm, *m_stackTrace.get());
        }
        m_stackTrace = nullptr;
    }
}

bool ErrorInstance::materializeErrorInfoIfNeeded(VM& vm)
{
    if (m_errorInfoMaterialized)
        return false;

    computeErrorInfo(vm, true);

    if (!m_stackString.isNull()) {
        auto attributes = static_cast<unsigned>(PropertyAttribute::DontEnum);

        putDirect(vm, vm.propertyNames->line, jsNumber(m_lineColumn.line), attributes);
        putDirect(vm, vm.propertyNames->column, jsNumber(m_lineColumn.column), attributes);
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
