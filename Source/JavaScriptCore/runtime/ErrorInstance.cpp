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
#include "CustomGetterSetter.h"
#include "ErrorInstanceInlines.h"
#include "InlineCallFrame.h"
#include "IntegrityInlines.h"
#include "Interpreter.h"
#include "JSCInlines.h"
#include "ParseInt.h"
#include "StackFrame.h"
#include <wtf/text/MakeString.h>

namespace JSC {

const ClassInfo ErrorInstance::s_info = { "Error"_s, &JSNonFinalObject::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ErrorInstance) };

static JSC_DECLARE_CUSTOM_GETTER(errorInstanceMaterializingStackGetter);

JSC_DEFINE_CUSTOM_GETTER(errorInstanceMaterializingStackGetter, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto* errorInstance = uncheckedDowncast<ErrorInstance>(JSValue::decode(thisValue));
    errorInstance->materializeErrorInfoIfNeeded(vm);
    return JSValue::encode(errorInstance->getDirect(vm, vm.propertyNames->stack));
}

ErrorInstance::ErrorInstance(VM& vm, Structure* structure, ErrorType errorType)
    : Base(vm, structure)
    , m_errorType(errorType)
    , m_stackOverflowError(false)
    , m_outOfMemoryError(false)
    , m_errorInfoMaterialized(false)
    , m_hasErrorInfo(false)
    , m_stackPropertyProvidedByCapturedStackTrace(false)
    , m_nativeGetterTypeError(false)
    , m_parseError(false)
#if ENABLE(WEBASSEMBLY)
    , m_catchableFromWasm(true)
#endif // ENABLE(WEBASSEMBLY)
{
}

ErrorInstance* ErrorInstance::create(JSGlobalObject* globalObject, String&& message, ErrorType errorType, LineColumn lineColumn, String&& sourceURL, String&& stackString, String&& cause)
{
    VM& vm = globalObject->vm();
    Structure* structure = globalObject->errorStructure(errorType);
    ErrorInstance* instance = new (NotNull, allocateCell<ErrorInstance>(vm)) ErrorInstance(vm, structure, errorType);
    instance->finishCreation(vm, WTF::move(message), lineColumn, WTF::move(sourceURL), WTF::move(stackString), WTF::move(cause));
    return instance;
}

ErrorInstance* ErrorInstance::create(JSGlobalObject* globalObject, Structure* structure, JSValue message, JSValue options, SourceAppender appender, RuntimeType type, ErrorType errorType, bool useCurrentFrame, JSCell* subclassCaller)
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

    return create(vm, structure, messageString, cause, appender, type, errorType, useCurrentFrame, subclassCaller);
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
        return appender(message, codeBlock->source().provider()->getRange(expressionStart, expressionStop), type, ErrorInstance::SourceTextWhereErrorOccurred::FoundExactSource);

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
    return appender(message, codeBlock->source().provider()->getRange(start, stop), type, ErrorInstance::SourceTextWhereErrorOccurred::FoundApproximateSource);
}

void ErrorInstance::finishCreation(VM& vm, const String& message, JSValue cause, SourceAppender appender, RuntimeType type, bool useCurrentFrame, JSCell* subclassCaller)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    m_sourceAppender = appender;
    m_runtimeTypeForCause = type;

    std::unique_ptr<Vector<StackFrame>> stackTrace = getStackTrace(vm, this, useCurrentFrame, nullptr, nullptr, subclassCaller);
    {
        Locker locker { cellLock() };
        m_stackTrace = WTF::move(stackTrace);
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

    if (!messageWithSource.isNull())
        putDirect(vm, vm.propertyNames->message, jsString(vm, WTF::move(messageWithSource)), static_cast<unsigned>(PropertyAttribute::DontEnum));

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
        m_stackTrace = WTF::move(stackTrace);
    }
    vm.writeBarrier(this);
    if (!message.isNull())
        putDirect(vm, vm.propertyNames->message, jsString(vm, message), static_cast<unsigned>(PropertyAttribute::DontEnum));

    if (!cause.isEmpty())
        putDirect(vm, vm.propertyNames->cause, cause, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

void ErrorInstance::finishCreation(VM& vm, String&& message, LineColumn lineColumn, String&& sourceURL, String&& stackString, String&& cause)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    m_lineColumn = lineColumn;
    m_sourceURL = WTF::move(sourceURL);
    m_stackString = WTF::move(stackString);
    m_hasErrorInfo = !m_stackString.isNull();
    if (!message.isNull())
        putDirect(vm, vm.propertyNames->message, jsString(vm, WTF::move(message)), static_cast<unsigned>(PropertyAttribute::DontEnum));
    if (!cause.isNull())
        putDirect(vm, vm.propertyNames->cause, jsString(vm, WTF::move(cause)), static_cast<unsigned>(PropertyAttribute::DontEnum));
}

void ErrorInstance::finishCreationForEmbedderError(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    std::unique_ptr<Vector<StackFrame>> stackTrace = getStackTrace(vm, this, /* useCurrentFrame */ true);
    {
        Locker locker { cellLock() };
        m_stackTrace = WTF::move(stackTrace);
    }
    vm.writeBarrier(this);

    // Deliberately add no own "message" / "cause" properties; the embedder exposes those itself.
}

void ErrorInstance::setErrorInfoForEmbedderError(LineColumn lineColumn, String&& sourceURL, String&& stackString)
{
    ASSERT(!m_errorInfoMaterialized);
    ASSERT(!m_capturedStackTrace);

    {
        Locker locker { cellLock() };
        m_stackTrace = nullptr;
    }
    m_lineColumn = lineColumn;
    m_sourceURL = WTF::move(sourceURL);
    m_stackString = WTF::move(stackString);
    m_hasErrorInfo = !m_stackString.isNull();
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
        JSObject* obj = uncheckedDowncast<JSObject>(currentObj);
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

    return makeString(nameString, nameString.isEmpty() || messageString.isEmpty() ? ""_s : ": "_s, messageString);
}

String ErrorInstance::tryGetMessageForDebugging()
{
    VM& vm = this->vm();

    JSValue messageValue;
    auto messagePropertName = vm.propertyNames->message;
    PropertySlot messageSlot(this, PropertySlot::InternalMethodType::VMInquiry, &vm);
    if (JSObject::getOwnNonIndexPropertySlot(vm, structure(), messagePropertName, messageSlot))
        messageValue = messageSlot.getPureResult();

    if (JSString* string = dynamicDowncast<JSString>(messageValue))
        return string->tryGetValue();
    return emptyString();
}

static bool hasUnmarkedFrame(VM& vm, const Vector<StackFrame>& frames)
{
    return std::ranges::any_of(frames, [&](const StackFrame& frame) {
        return !frame.isMarked(vm);
    });
}

void ErrorInstance::reconcileWeakReferencesAtGCEnd(VM& vm, CollectionScope)
{
    // We don't want to keep our stack traces alive forever if the user doesn't access the stack trace.
    // If we did, we might end up keeping functions (and their global objects) alive that happened to
    // get caught in a trace.
    // Since the frames are weak, a dead one means the trace can no longer be reconstructed, so
    // materialize it into strings while it is still readable.
    if (m_capturedStackTrace && hasUnmarkedFrame(vm, *m_capturedStackTrace)) {
        DeferGCForAWhile deferGC(vm);
        m_stackString = Interpreter::stackTraceAsString(vm, *m_capturedStackTrace);
        Locker locker { cellLock() };
        m_capturedStackTrace->clear();
    }

    if (m_stackTrace && hasUnmarkedFrame(vm, *m_stackTrace))
        computeErrorInfo(vm);
}

void ErrorInstance::computeErrorInfo(VM& vm)
{
    ASSERT(!m_errorInfoMaterialized);
    // Here we use DeferGCForAWhile instead of DeferGC since GC's Heap::runEndPhase can trigger this function. In
    // that case, DeferGC's destructor might trigger another GC cycle which is unexpected.
    DeferGCForAWhile deferGC(vm);

    if (m_stackTrace && !m_stackTrace->isEmpty()) {
        getLineColumnAndSource(vm, m_stackTrace.get(), m_lineColumn, m_sourceURL);
        if (!m_stackPropertyProvidedByCapturedStackTrace)
            m_stackString = Interpreter::stackTraceAsString(vm, *m_stackTrace.get());
        m_hasErrorInfo = true;
        m_stackTrace = nullptr;
    }
}

bool ErrorInstance::materializeErrorInfoIfNeeded(VM& vm)
{
    bool materializedCapturedStackProperty = materializeCapturedStackPropertyIfSaved(vm);
    if (m_errorInfoMaterialized)
        return materializedCapturedStackProperty;

    computeErrorInfo(vm);

    if (m_hasErrorInfo) {
        auto attributes = static_cast<unsigned>(PropertyAttribute::DontEnum);

        putDirect(vm, vm.propertyNames->line, jsNumber(m_lineColumn.line), attributes);
        putDirect(vm, vm.propertyNames->column, jsNumber(m_lineColumn.column), attributes);
        if (!m_sourceURL.isEmpty())
            putDirect(vm, vm.propertyNames->sourceURL, jsString(vm, WTF::move(m_sourceURL)), attributes);

        if (!m_stackPropertyProvidedByCapturedStackTrace)
            putDirect(vm, vm.propertyNames->stack, jsString(vm, WTF::move(m_stackString)), attributes);
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

bool ErrorInstance::trySaveCapturedStackTraceForLazyMaterialization(VM& vm, Vector<StackFrame>& stackTrace)
{
    bool needsPlaceholder = !m_capturedStackTrace;
    if (needsPlaceholder && (isValidOffset(getDirectOffset(vm, vm.propertyNames->stack)) || !isStructureExtensible()))
        return false;
    auto capturedStackTrace = makeUnique<Vector<StackFrame>>(WTF::move(stackTrace));
    {
        Locker locker { cellLock() };
        m_capturedStackTrace = WTF::move(capturedStackTrace);
    }
    m_stackString = String();
    if (needsPlaceholder)
        putDirectCustomAccessor(vm, vm.propertyNames->stack, CustomGetterSetter::create(vm, errorInstanceMaterializingStackGetter, nullptr), PropertyAttribute::DontEnum | PropertyAttribute::CustomValue);
    vm.writeBarrier(this);
    return true;
}

bool ErrorInstance::materializeCapturedStackPropertyIfSaved(VM& vm)
{
    if (!m_capturedStackTrace)
        return false;

    std::unique_ptr<Vector<StackFrame>> capturedStackTrace;
    {
        Locker locker { cellLock() };
        capturedStackTrace = WTF::move(m_capturedStackTrace);
    }
    String stackString = WTF::move(m_stackString);
    if (stackString.isNull()) {
        // The frames are weak, so a collection while formatting could free what they point to.
        DeferGCForAWhile deferGC(vm);
        stackString = Interpreter::stackTraceAsString(vm, *capturedStackTrace);
    }
    putDirect(vm, vm.propertyNames->stack, jsString(vm, WTF::move(stackString)), static_cast<unsigned>(PropertyAttribute::DontEnum));
    return true;
}

bool ErrorInstance::getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = globalObject->vm();
    ErrorInstance* thisObject = uncheckedDowncast<ErrorInstance>(object);
    thisObject->materializeErrorInfoIfNeeded(vm, propertyName);
    return Base::getOwnPropertySlot(thisObject, globalObject, propertyName, slot);
}

void ErrorInstance::getOwnSpecialPropertyNames(JSObject* object, JSGlobalObject* globalObject, PropertyNameArrayBuilder&, DontEnumPropertiesMode mode)
{
    VM& vm = globalObject->vm();
    ErrorInstance* thisObject = uncheckedDowncast<ErrorInstance>(object);
    if (mode == DontEnumPropertiesMode::Include)
        thisObject->materializeErrorInfoIfNeeded(vm);
}

bool ErrorInstance::defineOwnProperty(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    VM& vm = globalObject->vm();
    ErrorInstance* thisObject = uncheckedDowncast<ErrorInstance>(object);
    thisObject->materializeErrorInfoIfNeeded(vm, propertyName);
    return Base::defineOwnProperty(thisObject, globalObject, propertyName, descriptor, shouldThrow);
}

bool ErrorInstance::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = globalObject->vm();
    ErrorInstance* thisObject = uncheckedDowncast<ErrorInstance>(cell);
    bool materializedProperties = thisObject->materializeErrorInfoIfNeeded(vm, propertyName);
    if (materializedProperties)
        slot.disableCaching();
    return Base::put(thisObject, globalObject, propertyName, value, slot);
}

bool ErrorInstance::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    VM& vm = globalObject->vm();
    ErrorInstance* thisObject = uncheckedDowncast<ErrorInstance>(cell);
    bool materializedProperties = thisObject->materializeErrorInfoIfNeeded(vm, propertyName);
    if (materializedProperties)
        slot.disableCaching();
    return Base::deleteProperty(thisObject, globalObject, propertyName, slot);
}

} // namespace JSC
