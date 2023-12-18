/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorRuntimeAgent.h"

#include "Completion.h"
#include "ControlFlowProfiler.h"
#include "Debugger.h"
#include "InjectedScript.h"
#include "InjectedScriptHost.h"
#include "InjectedScriptManager.h"
#include "JSLock.h"
#include "ParserError.h"
#include "SourceCode.h"
#include "TypeProfiler.h"
#include "TypeProfilerLog.h"
#include <wtf/JSONValues.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace Inspector {

using namespace JSC;

WTF_MAKE_TZONE_ALLOCATED_IMPL(InspectorRuntimeAgent);

InspectorRuntimeAgent::InspectorRuntimeAgent(AgentContext& context)
    : InspectorAgentBase("Runtime"_s)
    , m_injectedScriptManager(context.injectedScriptManager)
    , m_debugger(*context.environment.debugger())
    , m_vm(context.environment.vm())
{
}

InspectorRuntimeAgent::~InspectorRuntimeAgent()
{
}

static Ref<Protocol::Runtime::ErrorRange> buildErrorRangeObject(const JSTokenLocation& tokenLocation)
{
    return Protocol::Runtime::ErrorRange::create()
        .setStartOffset(tokenLocation.startOffset)
        .setEndOffset(tokenLocation.endOffset)
        .release();
}

Protocol::ErrorStringOr<void> InspectorRuntimeAgent::enable()
{
    m_enabled = true;

    return { };
}

Protocol::ErrorStringOr<void> InspectorRuntimeAgent::disable()
{
    m_enabled = false;

    return { };
}

Protocol::ErrorStringOr<std::tuple<Protocol::Runtime::SyntaxErrorType, String /* message */, RefPtr<Protocol::Runtime::ErrorRange>>> InspectorRuntimeAgent::parse(const String& expression)
{
    JSLockHolder lock(m_vm);

    ParserError error;
    checkSyntax(m_vm, JSC::makeSource(expression, { }, SourceTaintedOrigin::Untainted), error);

    std::optional<Protocol::Runtime::SyntaxErrorType> result;
    String message;
    RefPtr<Protocol::Runtime::ErrorRange> range;

    switch (error.syntaxErrorType()) {
    case ParserError::SyntaxErrorNone:
        result = Protocol::Runtime::SyntaxErrorType::None;
        break;

    case ParserError::SyntaxErrorIrrecoverable:
        result = Protocol::Runtime::SyntaxErrorType::Irrecoverable;
        break;

    case ParserError::SyntaxErrorUnterminatedLiteral:
        result = Protocol::Runtime::SyntaxErrorType::UnterminatedLiteral;
        break;

    case ParserError::SyntaxErrorRecoverable:
        result = Protocol::Runtime::SyntaxErrorType::Recoverable;
        break;
    }

    if (error.syntaxErrorType() != ParserError::SyntaxErrorNone) {
        message = error.message();
        range = buildErrorRangeObject(error.token().m_location);
    }

    return { { *result, message, WTFMove(range) } };
}

Protocol::ErrorStringOr<std::tuple<Ref<Protocol::Runtime::RemoteObject>, std::optional<bool> /* wasThrown */, std::optional<int> /* savedResultIndex */>> InspectorRuntimeAgent::evaluate(const String& expression, const String& objectGroup, std::optional<bool>&& includeCommandLineAPI, std::optional<bool>&& doNotPauseOnExceptionsAndMuteConsole, std::optional<Protocol::Runtime::ExecutionContextId>&& executionContextId, std::optional<bool>&& returnByValue, std::optional<bool>&& generatePreview, std::optional<bool>&& saveResult, std::optional<bool>&& emulateUserGesture)
{
    Protocol::ErrorString errorString;

    InjectedScript injectedScript = injectedScriptForEval(errorString, WTFMove(executionContextId));
    if (injectedScript.hasNoValue())
        return makeUnexpected(errorString);

    return evaluate(injectedScript, expression, objectGroup, WTFMove(includeCommandLineAPI), WTFMove(doNotPauseOnExceptionsAndMuteConsole), WTFMove(returnByValue), WTFMove(generatePreview), WTFMove(saveResult), WTFMove(emulateUserGesture));
}

Protocol::ErrorStringOr<std::tuple<Ref<Protocol::Runtime::RemoteObject>, std::optional<bool> /* wasThrown */, std::optional<int> /* savedResultIndex */>> InspectorRuntimeAgent::evaluate(InjectedScript& injectedScript, const String& expression, const String& objectGroup, std::optional<bool>&& includeCommandLineAPI, std::optional<bool>&& doNotPauseOnExceptionsAndMuteConsole, std::optional<bool>&& returnByValue, std::optional<bool>&& generatePreview, std::optional<bool>&& saveResult, std::optional<bool>&& /* emulateUserGesture */)
{
    ASSERT(!injectedScript.hasNoValue());

    Protocol::ErrorString errorString;

    RefPtr<Protocol::Runtime::RemoteObject> result;
    std::optional<bool> wasThrown;
    std::optional<int> savedResultIndex;

    JSC::Debugger::TemporarilyDisableExceptionBreakpoints temporarilyDisableExceptionBreakpoints(m_debugger);

    bool pauseAndMute = doNotPauseOnExceptionsAndMuteConsole.value_or(false);
    if (pauseAndMute) {
        temporarilyDisableExceptionBreakpoints.replace();
        muteConsole();
    }

    injectedScript.evaluate(errorString, expression, objectGroup, includeCommandLineAPI.value_or(false), returnByValue.value_or(false), generatePreview.value_or(false), saveResult.value_or(false), result, wasThrown, savedResultIndex);

    if (pauseAndMute)
        unmuteConsole();

    if (!result)
        return makeUnexpected(errorString);

    return { {result.releaseNonNull(), WTFMove(wasThrown), WTFMove(savedResultIndex) } };
}

void InspectorRuntimeAgent::awaitPromise(const Protocol::Runtime::RemoteObjectId& promiseObjectId, std::optional<bool>&& returnByValue, std::optional<bool>&& generatePreview, std::optional<bool>&& saveResult, Ref<AwaitPromiseCallback>&& callback)
{
    InjectedScript injectedScript = m_injectedScriptManager.injectedScriptForObjectId(promiseObjectId);
    if (injectedScript.hasNoValue()) {
        callback->sendFailure("Missing injected script for given promiseObjectId"_s);
        return;
    }

    injectedScript.awaitPromise(promiseObjectId, returnByValue.value_or(false), generatePreview.value_or(false), saveResult.value_or(false), [callback = WTFMove(callback)] (Protocol::ErrorString& errorString, RefPtr<Protocol::Runtime::RemoteObject>&& result, std::optional<bool>&& wasThrown, std::optional<int>&& savedResultIndex) {
        if (!result)
            callback->sendFailure(errorString);
        else
            callback->sendSuccess(result.releaseNonNull(), WTFMove(wasThrown), WTFMove(savedResultIndex));
    });
}

void InspectorRuntimeAgent::callFunctionOn(const Protocol::Runtime::RemoteObjectId& objectId, const String& functionDeclaration, RefPtr<JSON::Array>&& arguments, std::optional<bool>&& doNotPauseOnExceptionsAndMuteConsole, std::optional<bool>&& returnByValue, std::optional<bool>&& generatePreview, std::optional<bool>&& emulateUserGesture, std::optional<bool>&& awaitPromise, Ref<CallFunctionOnCallback>&& callback)
{
    InjectedScript injectedScript = m_injectedScriptManager.injectedScriptForObjectId(objectId);
    if (injectedScript.hasNoValue()) {
        callback->sendFailure("Missing injected script for given objectId"_s);
        return;
    }

    callFunctionOn(injectedScript, objectId, functionDeclaration, WTFMove(arguments), WTFMove(doNotPauseOnExceptionsAndMuteConsole), WTFMove(returnByValue), WTFMove(generatePreview), WTFMove(emulateUserGesture), WTFMove(awaitPromise), WTFMove(callback));
}

void InspectorRuntimeAgent::callFunctionOn(InjectedScript& injectedScript, const Protocol::Runtime::RemoteObjectId& objectId, const String& functionDeclaration, RefPtr<JSON::Array>&& arguments, std::optional<bool>&& doNotPauseOnExceptionsAndMuteConsole, std::optional<bool>&& returnByValue, std::optional<bool>&& generatePreview, std::optional<bool>&& /* emulateUserGesture */, std::optional<bool>&& awaitPromise, Ref<CallFunctionOnCallback>&& callback)
{
    ASSERT(!injectedScript.hasNoValue());

    JSC::Debugger::TemporarilyDisableExceptionBreakpoints temporarilyDisableExceptionBreakpoints(m_debugger);

    bool pauseAndMute = doNotPauseOnExceptionsAndMuteConsole.value_or(false);
    if (pauseAndMute) {
        temporarilyDisableExceptionBreakpoints.replace();
        muteConsole();
    }

    injectedScript.callFunctionOn(objectId, functionDeclaration, arguments ? arguments->toJSONString() : nullString(), returnByValue.value_or(false), generatePreview.value_or(false), awaitPromise.value_or(false), [callback = WTFMove(callback)] (Protocol::ErrorString& errorString, RefPtr<Protocol::Runtime::RemoteObject>&& result, std::optional<bool>&& wasThrown, std::optional<int>&& savedResultIndex) {
        UNUSED_PARAM(savedResultIndex);
        if (!result)
            callback->sendFailure(errorString);
        else
            callback->sendSuccess(result.releaseNonNull(), WTFMove(wasThrown));
    });

    if (pauseAndMute)
        unmuteConsole();
}

Protocol::ErrorStringOr<Ref<Protocol::Runtime::ObjectPreview>> InspectorRuntimeAgent::getPreview(const Protocol::Runtime::RemoteObjectId& objectId)
{
    Protocol::ErrorString errorString;

    InjectedScript injectedScript = m_injectedScriptManager.injectedScriptForObjectId(objectId);
    if (injectedScript.hasNoValue())
        return makeUnexpected("Missing injected script for given objectId"_s);

    JSC::Debugger::TemporarilyDisableExceptionBreakpoints temporarilyDisableExceptionBreakpoints(m_debugger);
    temporarilyDisableExceptionBreakpoints.replace();

    RefPtr<Protocol::Runtime::ObjectPreview> preview;

    muteConsole();

    injectedScript.getPreview(errorString, objectId, preview);

    unmuteConsole();

    if (!preview)
        return makeUnexpected(errorString);

    return preview.releaseNonNull();
}

Protocol::ErrorStringOr<std::tuple<Ref<JSON::ArrayOf<Protocol::Runtime::PropertyDescriptor>>, RefPtr<JSON::ArrayOf<Protocol::Runtime::InternalPropertyDescriptor>>>> InspectorRuntimeAgent::getProperties(const Protocol::Runtime::RemoteObjectId& objectId, std::optional<bool>&& ownProperties, std::optional<int>&& fetchStart, std::optional<int>&& fetchCount, std::optional<bool>&& generatePreview)
{
    Protocol::ErrorString errorString;

    InjectedScript injectedScript = m_injectedScriptManager.injectedScriptForObjectId(objectId);
    if (injectedScript.hasNoValue())
        return makeUnexpected("Missing injected script for given objectId"_s);

    int start = fetchStart.value_or(0);
    if (start < 0)
        return makeUnexpected("fetchStart cannot be negative"_s);

    int count = fetchCount.value_or(0);
    if (count < 0)
        return makeUnexpected("fetchCount cannot be negative"_s);

    RefPtr<JSON::ArrayOf<Protocol::Runtime::PropertyDescriptor>> properties;
    RefPtr<JSON::ArrayOf<Protocol::Runtime::InternalPropertyDescriptor>> internalProperties;

    JSC::Debugger::TemporarilyDisableExceptionBreakpoints temporarilyDisableExceptionBreakpoints(m_debugger);
    temporarilyDisableExceptionBreakpoints.replace();

    muteConsole();

    injectedScript.getProperties(errorString, objectId, ownProperties.value_or(false), start, count, generatePreview.value_or(false), properties);

    // Only include internal properties for the first fetch.
    if (!start)
        injectedScript.getInternalProperties(errorString, objectId, generatePreview.value_or(false), internalProperties);

    unmuteConsole();

    if (!properties)
        return makeUnexpected(errorString);

    return { { properties.releaseNonNull(), WTFMove(internalProperties) } };
}

Protocol::ErrorStringOr<std::tuple<Ref<JSON::ArrayOf<Protocol::Runtime::PropertyDescriptor>>, RefPtr<JSON::ArrayOf<Protocol::Runtime::InternalPropertyDescriptor>>>> InspectorRuntimeAgent::getDisplayableProperties(const Protocol::Runtime::RemoteObjectId& objectId, std::optional<int>&& fetchStart, std::optional<int>&& fetchCount, std::optional<bool>&& generatePreview)
{
    Protocol::ErrorString errorString;

    InjectedScript injectedScript = m_injectedScriptManager.injectedScriptForObjectId(objectId);
    if (injectedScript.hasNoValue())
        return makeUnexpected("Missing injected script for given objectId"_s);

    int start = fetchStart.value_or(0);
    if (start < 0)
        return makeUnexpected("fetchStart cannot be negative"_s);

    int count = fetchCount.value_or(0);
    if (count < 0)
        return makeUnexpected("fetchCount cannot be negative"_s);

    RefPtr<JSON::ArrayOf<Protocol::Runtime::PropertyDescriptor>> properties;
    RefPtr<JSON::ArrayOf<Protocol::Runtime::InternalPropertyDescriptor>> internalProperties;

    JSC::Debugger::TemporarilyDisableExceptionBreakpoints temporarilyDisableExceptionBreakpoints(m_debugger);
    temporarilyDisableExceptionBreakpoints.replace();

    muteConsole();

    injectedScript.getDisplayableProperties(errorString, objectId, start, count, generatePreview.value_or(false), properties);

    // Only include internal properties for the first fetch.
    if (!start)
        injectedScript.getInternalProperties(errorString, objectId, generatePreview.value_or(false), internalProperties);

    unmuteConsole();

    if (!properties)
        return makeUnexpected(errorString);

    return { { properties.releaseNonNull(), WTFMove(internalProperties) } };
}

Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Protocol::Runtime::CollectionEntry>>> InspectorRuntimeAgent::getCollectionEntries(const Protocol::Runtime::RemoteObjectId& objectId, const String& objectGroup, std::optional<int>&& fetchStart, std::optional<int>&& fetchCount)
{
    Protocol::ErrorString errorString;

    InjectedScript injectedScript = m_injectedScriptManager.injectedScriptForObjectId(objectId);
    if (injectedScript.hasNoValue())
        return makeUnexpected("Missing injected script for given objectId"_s);

    int start = fetchStart.value_or(0);
    if (start < 0)
        return makeUnexpected("fetchStart cannot be negative"_s);

    int count = fetchCount.value_or(0);
    if (count < 0)
        return makeUnexpected("fetchCount cannot be negative"_s);

    RefPtr<JSON::ArrayOf<Protocol::Runtime::CollectionEntry>> entries;

    injectedScript.getCollectionEntries(errorString, objectId, objectGroup, start, count, entries);

    if (!entries)
        return makeUnexpected(errorString);

    return entries.releaseNonNull();
}

Protocol::ErrorStringOr<std::optional<int> /* saveResultIndex */> InspectorRuntimeAgent::saveResult(Ref<JSON::Object>&& callArgument, std::optional<Protocol::Runtime::ExecutionContextId>&& executionContextId)
{
    Protocol::ErrorString errorString;

    InjectedScript injectedScript;

    auto objectId = callArgument->getString(Protocol::Runtime::CallArgument::objectIdKey);
    if (!objectId) {
        injectedScript = injectedScriptForEval(errorString, WTFMove(executionContextId));
        if (injectedScript.hasNoValue())
            return makeUnexpected(errorString);
    } else {
        injectedScript = m_injectedScriptManager.injectedScriptForObjectId(objectId);
        if (injectedScript.hasNoValue())
            return makeUnexpected("Missing injected script for given objectId"_s);
    }

    std::optional<int> savedResultIndex;

    injectedScript.saveResult(errorString, callArgument->toJSONString(), savedResultIndex);

    if (!savedResultIndex)
        return makeUnexpected(errorString);

    return savedResultIndex;
}

Protocol::ErrorStringOr<void> InspectorRuntimeAgent::setSavedResultAlias(const String& savedResultAlias)
{
    m_injectedScriptManager.injectedScriptHost().setSavedResultAlias(savedResultAlias);

    return { };
}

Protocol::ErrorStringOr<void> InspectorRuntimeAgent::releaseObject(const Protocol::Runtime::RemoteObjectId& objectId)
{
    InjectedScript injectedScript = m_injectedScriptManager.injectedScriptForObjectId(objectId);
    if (!injectedScript.hasNoValue())
        injectedScript.releaseObject(objectId);

    return { };
}

Protocol::ErrorStringOr<void> InspectorRuntimeAgent::releaseObjectGroup(const String& objectGroup)
{
    m_injectedScriptManager.releaseObjectGroup(objectGroup);

    return { };
}

Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Protocol::Runtime::TypeDescription>>> InspectorRuntimeAgent::getRuntimeTypesForVariablesAtOffsets(Ref<JSON::Array>&& locations)
{
    static constexpr bool verbose = false;

    if (!m_vm.typeProfiler())
        return makeUnexpected("VM has no type information"_s);

    auto types = JSON::ArrayOf<Protocol::Runtime::TypeDescription>::create();

    MonotonicTime start = MonotonicTime::now();
    m_vm.typeProfilerLog()->processLogEntries(m_vm, "User Query"_s);

    for (size_t i = 0; i < locations->length(); i++) {
        auto location = locations->get(i)->asObject();
        if (!location)
            return makeUnexpected("Unexpected non-object item in locations"_s);

        auto descriptor = location->getInteger(Protocol::Runtime::TypeLocation::typeInformationDescriptorKey).value_or(TypeProfilerSearchDescriptorNormal);
        auto sourceIDString = location->getString(Protocol::Runtime::TypeLocation::sourceIDKey);
        auto divot = location->getInteger(Protocol::Runtime::TypeLocation::divotKey).value_or(0);

        auto typeLocation = m_vm.typeProfiler()->findLocation(divot, parseInteger<uintptr_t>(sourceIDString).value(), static_cast<TypeProfilerSearchDescriptor>(descriptor), m_vm);

        RefPtr<TypeSet> typeSet;
        if (typeLocation) {
            if (typeLocation->m_globalTypeSet && typeLocation->m_globalVariableID != TypeProfilerNoGlobalIDExists)
                typeSet = typeLocation->m_globalTypeSet;
            else
                typeSet = typeLocation->m_instructionTypeSet;
        }

        bool isValid = typeLocation && typeSet && !typeSet->isEmpty();
        auto description = Protocol::Runtime::TypeDescription::create()
            .setIsValid(isValid)
            .release();

        if (isValid) {
            description->setLeastCommonAncestor(typeSet->leastCommonAncestor());
            description->setStructures(typeSet->allStructureRepresentations());
            description->setTypeSet(typeSet->inspectorTypeSet());
            description->setIsTruncated(typeSet->isOverflown());
        }

        types->addItem(WTFMove(description));
    }

    MonotonicTime end = MonotonicTime::now();
    if (verbose)
        dataLogF("Inspector::getRuntimeTypesForVariablesAtOffsets took %lfms\n", (end - start).milliseconds());

    return types;
}

void InspectorRuntimeAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
}

void InspectorRuntimeAgent::willDestroyFrontendAndBackend(DisconnectReason reason)
{
    if (reason != DisconnectReason::InspectedTargetDestroyed && m_isTypeProfilingEnabled)
        setTypeProfilerEnabledState(false);

    disable();
}

Protocol::ErrorStringOr<void> InspectorRuntimeAgent::enableTypeProfiler()
{
    setTypeProfilerEnabledState(true);

    return { };
}

Protocol::ErrorStringOr<void> InspectorRuntimeAgent::disableTypeProfiler()
{
    setTypeProfilerEnabledState(false);

    return { };
}

Protocol::ErrorStringOr<void> InspectorRuntimeAgent::enableControlFlowProfiler()
{
    setControlFlowProfilerEnabledState(true);

    return { };
}

Protocol::ErrorStringOr<void> InspectorRuntimeAgent::disableControlFlowProfiler()
{
    setControlFlowProfilerEnabledState(false);

    return { };
}

void InspectorRuntimeAgent::setTypeProfilerEnabledState(bool isTypeProfilingEnabled)
{
    if (m_isTypeProfilingEnabled == isTypeProfilingEnabled)
        return;
    m_isTypeProfilingEnabled = isTypeProfilingEnabled;

    VM& vm = m_vm;
    vm.whenIdle([&vm, isTypeProfilingEnabled] () {
        bool shouldRecompileFromTypeProfiler = (isTypeProfilingEnabled ? vm.enableTypeProfiler() : vm.disableTypeProfiler());
        if (shouldRecompileFromTypeProfiler)
            vm.deleteAllCode(PreventCollectionAndDeleteAllCode);
    });
}

void InspectorRuntimeAgent::setControlFlowProfilerEnabledState(bool isControlFlowProfilingEnabled)
{
    if (m_isControlFlowProfilingEnabled == isControlFlowProfilingEnabled)
        return;
    m_isControlFlowProfilingEnabled = isControlFlowProfilingEnabled;

    VM& vm = m_vm;
    vm.whenIdle([&vm, isControlFlowProfilingEnabled] () {
        bool shouldRecompileFromControlFlowProfiler = (isControlFlowProfilingEnabled ? vm.enableControlFlowProfiler() : vm.disableControlFlowProfiler());

        if (shouldRecompileFromControlFlowProfiler)
            vm.deleteAllCode(PreventCollectionAndDeleteAllCode);
    });
}

Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Protocol::Runtime::BasicBlock>>> InspectorRuntimeAgent::getBasicBlocks(const String& sourceID)
{
    if (!m_vm.controlFlowProfiler())
        return makeUnexpected("VM has no control flow information"_s);

    auto basicBlocks = JSON::ArrayOf<Protocol::Runtime::BasicBlock>::create();
    for (const auto& block : m_vm.controlFlowProfiler()->getBasicBlocksForSourceID(parseIntegerAllowingTrailingJunk<uintptr_t>(sourceID).value_or(0), m_vm)) {
        auto location = Protocol::Runtime::BasicBlock::create()
            .setStartOffset(block.m_startOffset)
            .setEndOffset(block.m_endOffset)
            .setHasExecuted(block.m_hasExecuted)
            .setExecutionCount(block.m_executionCount)
            .release();
        basicBlocks->addItem(WTFMove(location));
    }
    return basicBlocks;
}

} // namespace Inspector
