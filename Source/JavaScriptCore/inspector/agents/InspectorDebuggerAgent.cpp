/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2010, 2011 Google Inc. All rights reserved.
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

#include "config.h"
#include "InspectorDebuggerAgent.h"

#include "AsyncStackTrace.h"
#include "ContentSearchUtilities.h"
#include "Debugger.h"
#include "DebuggerScope.h"
#include "InjectedScript.h"
#include "InjectedScriptManager.h"
#include "JSJavaScriptCallFrame.h"
#include "JavaScriptCallFrame.h"
#include "RegularExpression.h"
#include "ScriptCallStack.h"
#include "ScriptCallStackFactory.h"
#include <wtf/Function.h>
#include <wtf/JSONValues.h>
#include <wtf/Stopwatch.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

const char* const InspectorDebuggerAgent::backtraceObjectGroup = "backtrace";

// Objects created and retained by evaluating breakpoint actions are put into object groups
// according to the breakpoint action identifier assigned by the frontend. A breakpoint may
// have several object groups, and objects from several backend breakpoint action instances may
// create objects in the same group.
static String objectGroupForBreakpointAction(JSC::BreakpointActionID id)
{
    return makeString("breakpoint-action-", id);
}

static bool isWebKitInjectedScript(const String& sourceURL)
{
    return sourceURL.startsWith("__InjectedScript_") && sourceURL.endsWith(".js");
}

static Optional<JSC::Breakpoint::Action::Type> breakpointActionTypeForString(ErrorString& errorString, const String& typeString)
{
    auto type = Inspector::Protocol::InspectorHelpers::parseEnumValueFromString<Inspector::Protocol::Debugger::BreakpointAction::Type>(typeString);
    if (!type) {
        errorString = makeString("Unknown breakpoint action type: "_s, typeString);
        return WTF::nullopt;
    }

    switch (*type) {
    case Inspector::Protocol::Debugger::BreakpointAction::Type::Log:
        return JSC::Breakpoint::Action::Type::Log;

    case Inspector::Protocol::Debugger::BreakpointAction::Type::Evaluate:
        return JSC::Breakpoint::Action::Type::Evaluate;

    case Inspector::Protocol::Debugger::BreakpointAction::Type::Sound:
        return JSC::Breakpoint::Action::Type::Sound;

    case Inspector::Protocol::Debugger::BreakpointAction::Type::Probe:
        return JSC::Breakpoint::Action::Type::Probe;
    }

    ASSERT_NOT_REACHED();
    return WTF::nullopt;
}

template <typename T>
static T parseBreakpointOptions(ErrorString& errorString, const JSON::Object* optionsPayload, Function<T(const String&, JSC::Breakpoint::ActionsVector&&, bool, size_t)> callback)
{
    String condition;
    JSC::Breakpoint::ActionsVector actions;
    bool autoContinue = false;
    size_t ignoreCount = 0;

    if (optionsPayload) {
        optionsPayload->getString("condition"_s, condition);

        RefPtr<JSON::Array> actionsPayload;
        optionsPayload->getArray("actions"_s, actionsPayload);
        if (auto count = actionsPayload ? actionsPayload->length() : 0) {
            actions.reserveCapacity(count);

            for (unsigned i = 0; i < count; ++i) {
                RefPtr<JSON::Object> actionObject;
                if (!actionsPayload->get(i)->asObject(actionObject)) {
                    errorString = "Unexpected non-object item in given actions"_s;
                    return { };
                }

                String actionTypeString;
                if (!actionObject->getString("type"_s, actionTypeString)) {
                    errorString = "Missing type for item in given actions"_s;
                    return { };
                }

                auto actionType = breakpointActionTypeForString(errorString, actionTypeString);
                if (!actionType)
                    return { };

                JSC::Breakpoint::Action action(*actionType);

                actionObject->getString("data"_s, action.data);

                // Specifying an identifier is optional. They are used to correlate probe samples
                // in the frontend across multiple backend probe actions and segregate object groups.
                actionObject->getInteger("id"_s, action.id);

                actions.append(WTFMove(action));
            }
        }

        optionsPayload->getBoolean("autoContinue"_s, autoContinue);
        optionsPayload->getInteger("ignoreCount"_s, ignoreCount);
    }

    return callback(condition, WTFMove(actions), autoContinue, ignoreCount);
}

Optional<InspectorDebuggerAgent::ProtocolBreakpoint> InspectorDebuggerAgent::ProtocolBreakpoint::fromPayload(ErrorString& errorString, JSC::SourceID sourceID, unsigned lineNumber, unsigned columnNumber, const JSON::Object* optionsPayload)
{
    return parseBreakpointOptions<Optional<ProtocolBreakpoint>>(errorString, optionsPayload, [&] (const String& condition, JSC::Breakpoint::ActionsVector&& actions, bool autoContinue, size_t ignoreCount) -> Optional<ProtocolBreakpoint> {
        return ProtocolBreakpoint(sourceID, lineNumber, columnNumber, condition, WTFMove(actions), autoContinue, ignoreCount);
    });
}

Optional<InspectorDebuggerAgent::ProtocolBreakpoint> InspectorDebuggerAgent::ProtocolBreakpoint::fromPayload(ErrorString& errorString, const String& url, bool isRegex, unsigned lineNumber, unsigned columnNumber, const JSON::Object* optionsPayload)
{
    return parseBreakpointOptions<Optional<ProtocolBreakpoint>>(errorString, optionsPayload, [&] (const String& condition, JSC::Breakpoint::ActionsVector&& actions, bool autoContinue, size_t ignoreCount) -> Optional<ProtocolBreakpoint> {
        return ProtocolBreakpoint(url, isRegex, lineNumber, columnNumber, condition, WTFMove(actions), autoContinue, ignoreCount);
    });
}

InspectorDebuggerAgent::ProtocolBreakpoint::ProtocolBreakpoint() = default;

InspectorDebuggerAgent::ProtocolBreakpoint::ProtocolBreakpoint(JSC::SourceID sourceID, unsigned lineNumber, unsigned columnNumber, const String& condition, JSC::Breakpoint::ActionsVector&& actions, bool autoContinue, size_t ignoreCount)
    : m_id(makeString(sourceID, ':', lineNumber, ':', columnNumber))
#if ASSERT_ENABLED
    , m_sourceID(sourceID)
#endif
    , m_lineNumber(lineNumber)
    , m_columnNumber(columnNumber)
    , m_condition(condition)
    , m_actions(WTFMove(actions))
    , m_autoContinue(autoContinue)
    , m_ignoreCount(ignoreCount)
{
}

InspectorDebuggerAgent::ProtocolBreakpoint::ProtocolBreakpoint(const String& url, bool isRegex, unsigned lineNumber, unsigned columnNumber, const String& condition, JSC::Breakpoint::ActionsVector&& actions, bool autoContinue, size_t ignoreCount)
    : m_id(makeString(isRegex ? "/" : "", url, isRegex ? "/" : "", ':', lineNumber, ':', columnNumber))
    , m_url(url)
    , m_isRegex(isRegex)
    , m_lineNumber(lineNumber)
    , m_columnNumber(columnNumber)
    , m_condition(condition)
    , m_actions(WTFMove(actions))
    , m_autoContinue(autoContinue)
    , m_ignoreCount(ignoreCount)
{
}

Ref<JSC::Breakpoint> InspectorDebuggerAgent::ProtocolBreakpoint::createDebuggerBreakpoint(JSC::BreakpointID debuggerBreakpointID, JSC::SourceID sourceID) const
{
    ASSERT(debuggerBreakpointID != JSC::noBreakpointID);
    ASSERT(sourceID != JSC::noSourceID);
    ASSERT(sourceID == m_sourceID || m_sourceID == JSC::noSourceID);

    auto debuggerBreakpoint = JSC::Breakpoint::create(debuggerBreakpointID, m_condition, copyToVector(m_actions), m_autoContinue, m_ignoreCount);
    debuggerBreakpoint->link(sourceID, m_lineNumber, m_columnNumber);
    return debuggerBreakpoint;
}

bool InspectorDebuggerAgent::ProtocolBreakpoint::matchesScriptURL(const String& scriptURL) const
{
    ASSERT(m_sourceID == JSC::noSourceID);

    if (m_isRegex) {
        JSC::Yarr::RegularExpression regex(m_url);
        return regex.match(scriptURL) != -1;
    }
    return m_url == scriptURL;
}

RefPtr<JSC::Breakpoint> InspectorDebuggerAgent::debuggerBreakpointFromPayload(ErrorString& errorString, const JSON::Object* optionsPayload)
{
    return parseBreakpointOptions<RefPtr<JSC::Breakpoint>>(errorString, optionsPayload, [] (const String& condition, JSC::Breakpoint::ActionsVector&& actions, bool autoContinue, size_t ignoreCount) {
        return JSC::Breakpoint::create(JSC::noBreakpointID, condition, WTFMove(actions), autoContinue, ignoreCount);
    });
}

InspectorDebuggerAgent::InspectorDebuggerAgent(AgentContext& context)
    : InspectorAgentBase("Debugger"_s)
    , m_frontendDispatcher(makeUnique<DebuggerFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(DebuggerBackendDispatcher::create(context.backendDispatcher, this))
    , m_scriptDebugServer(context.environment.scriptDebugServer())
    , m_injectedScriptManager(context.injectedScriptManager)
{
    // FIXME: make pauseReason optional so that there was no need to init it with "other".
    clearPauseDetails();
}

InspectorDebuggerAgent::~InspectorDebuggerAgent() = default;

void InspectorDebuggerAgent::didCreateFrontendAndBackend(FrontendRouter*, BackendDispatcher*)
{
}

void InspectorDebuggerAgent::willDestroyFrontendAndBackend(DisconnectReason reason)
{
    if (enabled())
        disable(reason == DisconnectReason::InspectedTargetDestroyed);
}

void InspectorDebuggerAgent::enable()
{
    m_enabled = true;

    m_scriptDebugServer.addObserver(*this);

    for (auto* listener : copyToVector(m_listeners))
        listener->debuggerWasEnabled();

    for (auto& [sourceID, script] : m_scripts) {
        Optional<JSC::Debugger::BlackboxType> blackboxType;
        if (isWebKitInjectedScript(script.sourceURL)) {
            if (!m_pauseForInternalScripts)
                blackboxType = JSC::Debugger::BlackboxType::Ignored;
        } else if (shouldBlackboxURL(script.sourceURL) || shouldBlackboxURL(script.url))
            blackboxType = JSC::Debugger::BlackboxType::Deferred;
        m_scriptDebugServer.setBlackboxType(sourceID, blackboxType);
    }
}

void InspectorDebuggerAgent::disable(bool isBeingDestroyed)
{
    for (auto* listener : copyToVector(m_listeners))
        listener->debuggerWasDisabled();

    m_scriptDebugServer.removeObserver(*this, isBeingDestroyed);

    clearInspectorBreakpointState();

    if (!isBeingDestroyed)
        m_scriptDebugServer.deactivateBreakpoints();

    clearAsyncStackTraceData();

    m_pauseOnAssertionFailures = false;
    m_pauseOnMicrotasks = false;

    m_enabled = false;
}

void InspectorDebuggerAgent::enable(ErrorString& errorString)
{
    if (enabled()) {
        errorString = "Debugger domain already enabled"_s;
        return;
    }

    enable();
}

void InspectorDebuggerAgent::disable(ErrorString&)
{
    disable(false);
}

bool InspectorDebuggerAgent::breakpointsActive() const
{
    return m_scriptDebugServer.breakpointsActive();
}

void InspectorDebuggerAgent::setAsyncStackTraceDepth(ErrorString& errorString, int depth)
{
    if (m_asyncStackTraceDepth == depth)
        return;

    if (depth < 0) {
        errorString = "Unexpected negative depth"_s;
        return;
    }

    m_asyncStackTraceDepth = depth;

    if (!m_asyncStackTraceDepth)
        clearAsyncStackTraceData();
}

void InspectorDebuggerAgent::setBreakpointsActive(ErrorString&, bool active)
{
    if (active)
        m_scriptDebugServer.activateBreakpoints();
    else
        m_scriptDebugServer.deactivateBreakpoints();
}

bool InspectorDebuggerAgent::isPaused() const
{
    return m_scriptDebugServer.isPaused();
}

void InspectorDebuggerAgent::setSuppressAllPauses(bool suppress)
{
    m_scriptDebugServer.setSuppressAllPauses(suppress);
}

void InspectorDebuggerAgent::updatePauseReasonAndData(DebuggerFrontendDispatcher::Reason reason, RefPtr<JSON::Object>&& data)
{
    if (m_pauseReason != DebuggerFrontendDispatcher::Reason::BlackboxedScript) {
        m_preBlackboxPauseReason = m_pauseReason;
        m_preBlackboxPauseData = WTFMove(m_pauseData);
    }

    m_pauseReason = reason;
    m_pauseData = WTFMove(data);
}

static RefPtr<JSON::Object> buildAssertPauseReason(const String& message)
{
    auto reason = Protocol::Debugger::AssertPauseReason::create().release();
    if (!message.isNull())
        reason->setMessage(message);
    return reason->openAccessors();
}

static RefPtr<JSON::Object> buildCSPViolationPauseReason(const String& directiveText)
{
    auto reason = Protocol::Debugger::CSPViolationPauseReason::create()
        .setDirective(directiveText)
        .release();
    return reason->openAccessors();
}

RefPtr<JSON::Object> InspectorDebuggerAgent::buildBreakpointPauseReason(JSC::BreakpointID debuggerBreakpointID)
{
    ASSERT(debuggerBreakpointID != JSC::noBreakpointID);

    for (auto& [protocolBreakpointID, debuggerBreakpoints] : m_debuggerBreakpointsForProtocolBreakpointID) {
        for (auto& debuggerBreakpoint : debuggerBreakpoints) {
            if (debuggerBreakpoint->id() == debuggerBreakpointID) {
                auto reason = Protocol::Debugger::BreakpointPauseReason::create()
                    .setBreakpointId(protocolBreakpointID)
                    .release();
                return reason->openAccessors();
            }
        }
    }

    return nullptr;
}

RefPtr<JSON::Object> InspectorDebuggerAgent::buildExceptionPauseReason(JSC::JSValue exception, const InjectedScript& injectedScript)
{
    ASSERT(exception);
    if (!exception)
        return nullptr;

    ASSERT(!injectedScript.hasNoValue());
    if (injectedScript.hasNoValue())
        return nullptr;

    return injectedScript.wrapObject(exception, InspectorDebuggerAgent::backtraceObjectGroup)->openAccessors();
}

void InspectorDebuggerAgent::handleConsoleAssert(const String& message)
{
    if (!breakpointsActive())
        return;

    if (m_pauseOnAssertionFailures)
        breakProgram(DebuggerFrontendDispatcher::Reason::Assert, buildAssertPauseReason(message));
}

InspectorDebuggerAgent::AsyncCallIdentifier InspectorDebuggerAgent::asyncCallIdentifier(AsyncCallType asyncCallType, int callbackId)
{
    return std::make_pair(static_cast<unsigned>(asyncCallType), callbackId);
}

void InspectorDebuggerAgent::didScheduleAsyncCall(JSC::JSGlobalObject* globalObject, AsyncCallType asyncCallType, int callbackId, bool singleShot)
{
    if (!m_asyncStackTraceDepth)
        return;

    if (!breakpointsActive())
        return;

    Ref<ScriptCallStack> callStack = createScriptCallStack(globalObject, m_asyncStackTraceDepth);
    ASSERT(callStack->size());
    if (!callStack->size())
        return;

    RefPtr<AsyncStackTrace> parentStackTrace;
    if (m_currentAsyncCallIdentifier) {
        auto it = m_pendingAsyncCalls.find(m_currentAsyncCallIdentifier.value());
        ASSERT(it != m_pendingAsyncCalls.end());
        parentStackTrace = it->value;
    }

    auto identifier = asyncCallIdentifier(asyncCallType, callbackId);
    auto asyncStackTrace = AsyncStackTrace::create(WTFMove(callStack), singleShot, WTFMove(parentStackTrace));

    m_pendingAsyncCalls.set(identifier, WTFMove(asyncStackTrace));
}

void InspectorDebuggerAgent::didCancelAsyncCall(AsyncCallType asyncCallType, int callbackId)
{
    if (!m_asyncStackTraceDepth)
        return;

    auto identifier = asyncCallIdentifier(asyncCallType, callbackId);
    auto it = m_pendingAsyncCalls.find(identifier);
    if (it == m_pendingAsyncCalls.end())
        return;

    auto& asyncStackTrace = it->value;
    asyncStackTrace->didCancelAsyncCall();

    if (m_currentAsyncCallIdentifier && m_currentAsyncCallIdentifier.value() == identifier)
        return;

    m_pendingAsyncCalls.remove(identifier);
}

void InspectorDebuggerAgent::willDispatchAsyncCall(AsyncCallType asyncCallType, int callbackId)
{
    if (!m_asyncStackTraceDepth)
        return;

    if (m_currentAsyncCallIdentifier)
        return;

    // A call can be scheduled before the Inspector is opened, or while async stack
    // traces are disabled. If no call data exists, do nothing.
    auto identifier = asyncCallIdentifier(asyncCallType, callbackId);
    auto it = m_pendingAsyncCalls.find(identifier);
    if (it == m_pendingAsyncCalls.end())
        return;

    auto& asyncStackTrace = it->value;
    asyncStackTrace->willDispatchAsyncCall(m_asyncStackTraceDepth);

    m_currentAsyncCallIdentifier = identifier;
}

void InspectorDebuggerAgent::didDispatchAsyncCall()
{
    if (!m_asyncStackTraceDepth)
        return;

    if (!m_currentAsyncCallIdentifier)
        return;

    auto identifier = m_currentAsyncCallIdentifier.value();
    auto it = m_pendingAsyncCalls.find(identifier);
    ASSERT(it != m_pendingAsyncCalls.end());

    auto& asyncStackTrace = it->value;
    asyncStackTrace->didDispatchAsyncCall();

    m_currentAsyncCallIdentifier = WTF::nullopt;

    if (!asyncStackTrace->isPending())
        m_pendingAsyncCalls.remove(identifier);
}

static RefPtr<Protocol::Debugger::Location> buildDebuggerLocation(const JSC::Breakpoint& debuggerBreakpoint)
{
    ASSERT(debuggerBreakpoint.isResolved());

    auto location = Protocol::Debugger::Location::create()
        .setScriptId(String::number(debuggerBreakpoint.sourceID()))
        .setLineNumber(debuggerBreakpoint.lineNumber())
        .release();
    location->setColumnNumber(debuggerBreakpoint.columnNumber());

    return location;
}

static bool parseLocation(ErrorString& errorString, const JSON::Object& location, JSC::SourceID& sourceID, unsigned& lineNumber, unsigned& columnNumber)
{
    if (!location.getInteger("lineNumber"_s, lineNumber)) {
        errorString = "Unexpected non-integer lineNumber in given location"_s;
        sourceID = JSC::noSourceID;
        return false;
    }

    String scriptIDStr;
    if (!location.getString("scriptId"_s, scriptIDStr)) {
        sourceID = JSC::noSourceID;
        errorString = "Unexepcted non-string scriptId in given location"_s;
        return false;
    }

    sourceID = scriptIDStr.toIntPtr();
    columnNumber = 0;
    location.getInteger("columnNumber"_s, columnNumber);
    return true;
}

void InspectorDebuggerAgent::setBreakpointByUrl(ErrorString& errorString, int lineNumber, const String* optionalURL, const String* optionalURLRegex, const int* optionalColumnNumber, const JSON::Object* optionsPayload, Protocol::Debugger::BreakpointId* outBreakpointId, RefPtr<JSON::ArrayOf<Protocol::Debugger::Location>>& outLocations)
{
    if (!optionalURL == !optionalURLRegex) {
        errorString = "Either url or urlRegex must be specified"_s;
        return;
    }

    auto protocolBreakpoint = ProtocolBreakpoint::fromPayload(errorString, optionalURL ? *optionalURL : *optionalURLRegex, !!optionalURLRegex, lineNumber, optionalColumnNumber ? *optionalColumnNumber : 0, optionsPayload);
    if (!protocolBreakpoint)
        return;

    if (m_protocolBreakpointForProtocolBreakpointID.contains(protocolBreakpoint->id())) {
        errorString = "Breakpoint for given location already exists."_s;
        return;
    }

    m_protocolBreakpointForProtocolBreakpointID.set(protocolBreakpoint->id(), *protocolBreakpoint);

    outLocations = JSON::ArrayOf<Protocol::Debugger::Location>::create();

    for (auto& [sourceID, script] : m_scripts) {
        String scriptURLForBreakpoints = !script.sourceURL.isEmpty() ? script.sourceURL : script.url;
        if (!protocolBreakpoint->matchesScriptURL(scriptURLForBreakpoints))
            continue;

        auto debuggerBreakpoint = protocolBreakpoint->createDebuggerBreakpoint(m_nextDebuggerBreakpointID++, sourceID);

        if (!resolveBreakpoint(script, debuggerBreakpoint))
            continue;

        if (!setBreakpoint(debuggerBreakpoint))
            continue;

        didSetBreakpoint(*protocolBreakpoint, debuggerBreakpoint);

        outLocations->addItem(buildDebuggerLocation(debuggerBreakpoint));
    }

    *outBreakpointId = protocolBreakpoint->id();
}

void InspectorDebuggerAgent::setBreakpoint(ErrorString& errorString, const JSON::Object& location, const JSON::Object* optionsPayload, Protocol::Debugger::BreakpointId* outBreakpointId, RefPtr<Protocol::Debugger::Location>& outActualLocation)
{
    JSC::SourceID sourceID;
    unsigned lineNumber;
    unsigned columnNumber;
    if (!parseLocation(errorString, location, sourceID, lineNumber, columnNumber))
        return;

    auto scriptIterator = m_scripts.find(sourceID);
    if (scriptIterator == m_scripts.end()) {
        errorString = "Missing script for scriptId in given location"_s;
        return;
    }

    auto protocolBreakpoint = ProtocolBreakpoint::fromPayload(errorString, sourceID, lineNumber, columnNumber, optionsPayload);
    if (!protocolBreakpoint)
        return;

    // Don't save `protocolBreakpoint` in `m_protocolBreakpointForProtocolBreakpointID` because it
    // was set specifically for the given `sourceID`, which is unique, meaning that it will never
    // be used inside `InspectorDebuggerAgent::didParseSource`.

    auto debuggerBreakpoint = protocolBreakpoint->createDebuggerBreakpoint(m_nextDebuggerBreakpointID++, sourceID);

    if (!resolveBreakpoint(scriptIterator->value, debuggerBreakpoint)) {
        errorString = "Could not resolve breakpoint"_s;
        return;
    }

    if (!setBreakpoint(debuggerBreakpoint)) {
        errorString = "Breakpoint for given location already exists"_s;
        return;
    }

    didSetBreakpoint(*protocolBreakpoint, debuggerBreakpoint);

    outActualLocation = buildDebuggerLocation(debuggerBreakpoint);
    *outBreakpointId = protocolBreakpoint->id();
}

void InspectorDebuggerAgent::didSetBreakpoint(ProtocolBreakpoint& protocolBreakpoint, JSC::Breakpoint& debuggerBreakpoint)
{
    auto& debuggerBreakpoints = m_debuggerBreakpointsForProtocolBreakpointID.ensure(protocolBreakpoint.id(), [] {
        return JSC::BreakpointsVector();
    }).iterator->value;

    debuggerBreakpoints.append(debuggerBreakpoint);
}

bool InspectorDebuggerAgent::resolveBreakpoint(const JSC::Debugger::Script& script, JSC::Breakpoint& debuggerBreakpoint)
{
    if (debuggerBreakpoint.lineNumber() < static_cast<unsigned>(script.startLine) || static_cast<unsigned>(script.endLine) < debuggerBreakpoint.lineNumber())
        return false;

    return m_scriptDebugServer.resolveBreakpoint(debuggerBreakpoint, script.sourceProvider.get());
}

bool InspectorDebuggerAgent::setBreakpoint(JSC::Breakpoint& debuggerBreakpoint)
{
    JSC::JSLockHolder locker(m_scriptDebugServer.vm());
    return m_scriptDebugServer.setBreakpoint(debuggerBreakpoint);
}

void InspectorDebuggerAgent::removeBreakpoint(ErrorString&, const Inspector::Protocol::Debugger::BreakpointId& protocolBreakpointID)
{
    m_protocolBreakpointForProtocolBreakpointID.remove(protocolBreakpointID);

    for (auto& debuggerBreakpoint : m_debuggerBreakpointsForProtocolBreakpointID.take(protocolBreakpointID)) {
        for (const auto& action : debuggerBreakpoint->actions())
            m_injectedScriptManager.releaseObjectGroup(objectGroupForBreakpointAction(action.id));

        JSC::JSLockHolder locker(m_scriptDebugServer.vm());
        m_scriptDebugServer.removeBreakpoint(debuggerBreakpoint);
    }
}

void InspectorDebuggerAgent::continueUntilNextRunLoop(ErrorString& errorString)
{
    if (!assertPaused(errorString))
        return;

    resume(errorString);

    m_enablePauseWhenIdle = true;

    registerIdleHandler();
}

void InspectorDebuggerAgent::continueToLocation(ErrorString& errorString, const JSON::Object& location)
{
    if (!assertPaused(errorString))
        return;

    if (m_continueToLocationDebuggerBreakpoint) {
        m_scriptDebugServer.removeBreakpoint(*m_continueToLocationDebuggerBreakpoint);
        m_continueToLocationDebuggerBreakpoint = nullptr;
    }

    JSC::SourceID sourceID;
    unsigned lineNumber;
    unsigned columnNumber;
    if (!parseLocation(errorString, location, sourceID, lineNumber, columnNumber))
        return;

    auto scriptIterator = m_scripts.find(sourceID);
    if (scriptIterator == m_scripts.end()) {
        m_scriptDebugServer.continueProgram();
        m_frontendDispatcher->resumed();
        errorString = "Missing script for scriptId in given location"_s;
        return;
    }

    auto protocolBreakpoint = ProtocolBreakpoint::fromPayload(errorString, sourceID, lineNumber, columnNumber);
    if (!protocolBreakpoint)
        return;

    // Don't save `protocolBreakpoint` in `m_protocolBreakpointForProtocolBreakpointID` because it
    // is a temporary breakpoint that will be removed as soon as `location` is reached.

    auto debuggerBreakpoint = protocolBreakpoint->createDebuggerBreakpoint(m_nextDebuggerBreakpointID++, sourceID);

    if (!resolveBreakpoint(scriptIterator->value, debuggerBreakpoint)) {
        m_scriptDebugServer.continueProgram();
        m_frontendDispatcher->resumed();
        errorString = "Could not resolve breakpoint"_s;
        return;
    }

    if (!setBreakpoint(debuggerBreakpoint)) {
        // There is an existing breakpoint at this location. Instead of
        // acting like a series of steps, just resume and we will either
        // hit this new breakpoint or not.
        m_scriptDebugServer.continueProgram();
        m_frontendDispatcher->resumed();
        return;
    }

    m_continueToLocationDebuggerBreakpoint = WTFMove(debuggerBreakpoint);

    // Treat this as a series of steps until reaching the new breakpoint.
    // So don't issue a resumed event unless we exit the VM without pausing.
    willStepAndMayBecomeIdle();
    m_scriptDebugServer.continueProgram();
}

void InspectorDebuggerAgent::searchInContent(ErrorString& errorString, const String& scriptIDStr, const String& query, const bool* optionalCaseSensitive, const bool* optionalIsRegex, RefPtr<JSON::ArrayOf<Protocol::GenericTypes::SearchMatch>>& results)
{
    JSC::SourceID sourceID = scriptIDStr.toIntPtr();
    auto it = m_scripts.find(sourceID);
    if (it == m_scripts.end()) {
        errorString = "Missing script for given scriptId";
        return;
    }

    bool isRegex = optionalIsRegex ? *optionalIsRegex : false;
    bool caseSensitive = optionalCaseSensitive ? *optionalCaseSensitive : false;
    results = ContentSearchUtilities::searchInTextByLines(it->value.source, query, caseSensitive, isRegex);
}

void InspectorDebuggerAgent::getScriptSource(ErrorString& errorString, const String& scriptIDStr, String* scriptSource)
{
    JSC::SourceID sourceID = scriptIDStr.toIntPtr();
    auto it = m_scripts.find(sourceID);
    if (it != m_scripts.end())
        *scriptSource = it->value.source;
    else
        errorString = "Missing script for given scriptId";
}

void InspectorDebuggerAgent::getFunctionDetails(ErrorString& errorString, const String& functionId, RefPtr<Protocol::Debugger::FunctionDetails>& details)
{
    InjectedScript injectedScript = m_injectedScriptManager.injectedScriptForObjectId(functionId);
    if (injectedScript.hasNoValue()) {
        errorString = "Missing injected script for given functionId"_s;
        return;
    }

    injectedScript.getFunctionDetails(errorString, functionId, details);
}

void InspectorDebuggerAgent::schedulePauseAtNextOpportunity(DebuggerFrontendDispatcher::Reason reason, RefPtr<JSON::Object>&& data)
{
    if (m_javaScriptPauseScheduled)
        return;

    m_javaScriptPauseScheduled = true;

    updatePauseReasonAndData(reason, WTFMove(data));

    JSC::JSLockHolder locker(m_scriptDebugServer.vm());
    m_scriptDebugServer.schedulePauseAtNextOpportunity();
}

void InspectorDebuggerAgent::cancelPauseAtNextOpportunity()
{
    if (!m_javaScriptPauseScheduled)
        return;

    m_javaScriptPauseScheduled = false;

    clearPauseDetails();
    m_scriptDebugServer.cancelPauseAtNextOpportunity();
    m_enablePauseWhenIdle = false;
}

bool InspectorDebuggerAgent::schedulePauseForSpecialBreakpoint(JSC::Breakpoint& breakpoint, DebuggerFrontendDispatcher::Reason reason, RefPtr<JSON::Object>&& data)
{
    JSC::JSLockHolder locker(m_scriptDebugServer.vm());

    if (!m_scriptDebugServer.schedulePauseForSpecialBreakpoint(breakpoint))
        return false;

    updatePauseReasonAndData(reason, WTFMove(data));
    return true;
}

bool InspectorDebuggerAgent::cancelPauseForSpecialBreakpoint(JSC::Breakpoint& breakpoint)
{
    if (!m_scriptDebugServer.cancelPauseForSpecialBreakpoint(breakpoint))
        return false;

    clearPauseDetails();
    return true;
}

void InspectorDebuggerAgent::pause(ErrorString&)
{
    schedulePauseAtNextOpportunity(DebuggerFrontendDispatcher::Reason::PauseOnNextStatement);
}

void InspectorDebuggerAgent::resume(ErrorString& errorString)
{
    if (!m_pausedGlobalObject && !m_javaScriptPauseScheduled) {
        errorString = "Must be paused or waiting to pause"_s;
        return;
    }

    cancelPauseAtNextOpportunity();
    m_scriptDebugServer.continueProgram();
    m_conditionToDispatchResumed = ShouldDispatchResumed::WhenContinued;
}

void InspectorDebuggerAgent::stepNext(ErrorString& errorString)
{
    if (!assertPaused(errorString))
        return;

    willStepAndMayBecomeIdle();
    m_scriptDebugServer.stepNextExpression();
}

void InspectorDebuggerAgent::stepOver(ErrorString& errorString)
{
    if (!assertPaused(errorString))
        return;

    willStepAndMayBecomeIdle();
    m_scriptDebugServer.stepOverStatement();
}

void InspectorDebuggerAgent::stepInto(ErrorString& errorString)
{
    if (!assertPaused(errorString))
        return;

    willStepAndMayBecomeIdle();
    m_scriptDebugServer.stepIntoStatement();
}

void InspectorDebuggerAgent::stepOut(ErrorString& errorString)
{
    if (!assertPaused(errorString))
        return;

    willStepAndMayBecomeIdle();
    m_scriptDebugServer.stepOutOfFunction();
}

void InspectorDebuggerAgent::registerIdleHandler()
{
    if (!m_registeredIdleCallback) {
        m_registeredIdleCallback = true;
        JSC::VM& vm = m_scriptDebugServer.vm();
        vm.whenIdle([this]() {
            didBecomeIdle();
        });
    }
}

void InspectorDebuggerAgent::willStepAndMayBecomeIdle()
{
    // When stepping the backend must eventually trigger a "paused" or "resumed" event.
    // If the step causes us to exit the VM, then we should issue "resumed".
    m_conditionToDispatchResumed = ShouldDispatchResumed::WhenIdle;

    registerIdleHandler();
}

void InspectorDebuggerAgent::didBecomeIdle()
{
    m_registeredIdleCallback = false;

    if (m_conditionToDispatchResumed == ShouldDispatchResumed::WhenIdle) {
        cancelPauseAtNextOpportunity();
        m_scriptDebugServer.continueProgram();
        m_frontendDispatcher->resumed();
    }

    m_conditionToDispatchResumed = ShouldDispatchResumed::No;

    if (m_enablePauseWhenIdle) {
        ErrorString ignored;
        pause(ignored);
    }
}

void InspectorDebuggerAgent::setPauseOnDebuggerStatements(ErrorString&, bool enabled)
{
    m_scriptDebugServer.setPauseOnDebuggerStatements(enabled);
}

void InspectorDebuggerAgent::setPauseOnExceptions(ErrorString& errorString, const String& stringPauseState)
{
    JSC::Debugger::PauseOnExceptionsState pauseState;
    if (stringPauseState == "none")
        pauseState = JSC::Debugger::DontPauseOnExceptions;
    else if (stringPauseState == "all")
        pauseState = JSC::Debugger::PauseOnAllExceptions;
    else if (stringPauseState == "uncaught")
        pauseState = JSC::Debugger::PauseOnUncaughtExceptions;
    else {
        errorString = makeString("Unknown state: "_s, stringPauseState);
        return;
    }

    m_scriptDebugServer.setPauseOnExceptionsState(static_cast<JSC::Debugger::PauseOnExceptionsState>(pauseState));
    if (m_scriptDebugServer.pauseOnExceptionsState() != pauseState)
        errorString = "Internal error. Could not change pause on exceptions state"_s;
}

void InspectorDebuggerAgent::setPauseOnAssertions(ErrorString&, bool enabled)
{
    m_pauseOnAssertionFailures = enabled;
}

void InspectorDebuggerAgent::setPauseOnMicrotasks(ErrorString&, bool enabled)
{
    m_pauseOnMicrotasks = enabled;
}

void InspectorDebuggerAgent::evaluateOnCallFrame(ErrorString& errorString, const String& callFrameId, const String& expression, const String* objectGroup, const bool* includeCommandLineAPI, const bool* doNotPauseOnExceptionsAndMuteConsole, const bool* returnByValue, const bool* generatePreview, const bool* saveResult, const bool* /* emulateUserGesture */, RefPtr<Protocol::Runtime::RemoteObject>& result, Optional<bool>& wasThrown, Optional<int>& savedResultIndex)
{
    if (!assertPaused(errorString))
        return;

    InjectedScript injectedScript = m_injectedScriptManager.injectedScriptForObjectId(callFrameId);
    if (injectedScript.hasNoValue()) {
        errorString = "Missing injected script for given callFrameId"_s;
        return;
    }

    auto pauseState = m_scriptDebugServer.pauseOnExceptionsState();
    bool pauseAndMute = doNotPauseOnExceptionsAndMuteConsole && *doNotPauseOnExceptionsAndMuteConsole;
    if (pauseAndMute) {
        if (pauseState != JSC::Debugger::DontPauseOnExceptions)
            m_scriptDebugServer.setPauseOnExceptionsState(JSC::Debugger::DontPauseOnExceptions);
        muteConsole();
    }

    injectedScript.evaluateOnCallFrame(errorString, m_currentCallStack.get(), callFrameId, expression,
        objectGroup ? *objectGroup : emptyString(), includeCommandLineAPI && *includeCommandLineAPI, returnByValue && *returnByValue, generatePreview && *generatePreview, saveResult && *saveResult,
        result, wasThrown, savedResultIndex);

    if (pauseAndMute) {
        unmuteConsole();
        m_scriptDebugServer.setPauseOnExceptionsState(pauseState);
    }
}

void InspectorDebuggerAgent::setShouldBlackboxURL(ErrorString& errorString, const String& url, bool shouldBlackbox, const bool* optionalCaseSensitive, const bool* optionalIsRegex)
{
    if (url.isEmpty()) {
        errorString = "URL must not be empty"_s;
        return;
    }

    bool caseSensitive = optionalCaseSensitive && *optionalCaseSensitive;
    bool isRegex = optionalIsRegex && *optionalIsRegex;

    if (!caseSensitive && !isRegex && isWebKitInjectedScript(url)) {
        errorString = "Blackboxing of internal scripts is controlled by 'Debugger.setPauseForInternalScripts'"_s;
        return;
    }

    BlackboxConfig config { url, caseSensitive, isRegex };
    if (shouldBlackbox)
        m_blackboxedURLs.appendIfNotContains(config);
    else
        m_blackboxedURLs.removeAll(config);

    for (auto& [sourceID, script] : m_scripts) {
        if (isWebKitInjectedScript(script.sourceURL))
            continue;

        Optional<JSC::Debugger::BlackboxType> blackboxType;
        if (shouldBlackboxURL(script.sourceURL) || shouldBlackboxURL(script.url))
            blackboxType = JSC::Debugger::BlackboxType::Deferred;
        m_scriptDebugServer.setBlackboxType(sourceID, blackboxType);
    }
}

bool InspectorDebuggerAgent::shouldBlackboxURL(const String& url) const
{
    if (!url.isEmpty()) {
        for (const auto& blackboxConfig : m_blackboxedURLs) {
            auto searchStringType = blackboxConfig.isRegex ? ContentSearchUtilities::SearchStringType::Regex : ContentSearchUtilities::SearchStringType::ExactString;
            auto regex = ContentSearchUtilities::createRegularExpressionForSearchString(blackboxConfig.url, blackboxConfig.caseSensitive, searchStringType);
            if (regex.match(url) != -1)
                return true;
        }
    }
    return false;
}

void InspectorDebuggerAgent::scriptExecutionBlockedByCSP(const String& directiveText)
{
    if (m_scriptDebugServer.pauseOnExceptionsState() != JSC::Debugger::DontPauseOnExceptions)
        breakProgram(DebuggerFrontendDispatcher::Reason::CSPViolation, buildCSPViolationPauseReason(directiveText));
}

Ref<JSON::ArrayOf<Protocol::Debugger::CallFrame>> InspectorDebuggerAgent::currentCallFrames(const InjectedScript& injectedScript)
{
    ASSERT(!injectedScript.hasNoValue());
    if (injectedScript.hasNoValue())
        return JSON::ArrayOf<Protocol::Debugger::CallFrame>::create();

    return injectedScript.wrapCallFrames(m_currentCallStack.get());
}

String InspectorDebuggerAgent::sourceMapURLForScript(const JSC::Debugger::Script& script)
{
    return script.sourceMappingURL;
}

void InspectorDebuggerAgent::setPauseForInternalScripts(ErrorString&, bool shouldPause)
{
    if (shouldPause == m_pauseForInternalScripts)
        return;

    m_pauseForInternalScripts = shouldPause;

    auto blackboxType = !m_pauseForInternalScripts ? Optional<JSC::Debugger::BlackboxType>(JSC::Debugger::BlackboxType::Ignored) : WTF::nullopt;
    for (auto& [sourceID, script] : m_scripts) {
        if (!isWebKitInjectedScript(script.sourceURL))
            continue;
        m_scriptDebugServer.setBlackboxType(sourceID, blackboxType);
    }
}

void InspectorDebuggerAgent::didParseSource(JSC::SourceID sourceID, const JSC::Debugger::Script& script)
{
    String scriptIDStr = String::number(sourceID);
    bool hasSourceURL = !script.sourceURL.isEmpty();
    String sourceURL = script.sourceURL;
    String sourceMappingURL = sourceMapURLForScript(script);

    const bool isModule = script.sourceProvider->sourceType() == JSC::SourceProviderSourceType::Module;
    const bool* isContentScript = script.isContentScript ? &script.isContentScript : nullptr;
    String* sourceURLParam = hasSourceURL ? &sourceURL : nullptr;
    String* sourceMapURLParam = sourceMappingURL.isEmpty() ? nullptr : &sourceMappingURL;

    m_frontendDispatcher->scriptParsed(scriptIDStr, script.url, script.startLine, script.startColumn, script.endLine, script.endColumn, isContentScript, sourceURLParam, sourceMapURLParam, isModule ? &isModule : nullptr);

    m_scripts.set(sourceID, script);

    if (isWebKitInjectedScript(sourceURL)) {
        if (!m_pauseForInternalScripts)
            m_scriptDebugServer.setBlackboxType(sourceID, JSC::Debugger::BlackboxType::Ignored);
    } else if (shouldBlackboxURL(sourceURL) || shouldBlackboxURL(script.url))
        m_scriptDebugServer.setBlackboxType(sourceID, JSC::Debugger::BlackboxType::Deferred);

    String scriptURLForBreakpoints = hasSourceURL ? script.sourceURL : script.url;
    if (scriptURLForBreakpoints.isEmpty())
        return;

    for (auto& protocolBreakpoint : m_protocolBreakpointForProtocolBreakpointID.values()) {
        if (!protocolBreakpoint.matchesScriptURL(scriptURLForBreakpoints))
            continue;

        auto debuggerBreakpoint = protocolBreakpoint.createDebuggerBreakpoint(m_nextDebuggerBreakpointID++, sourceID);

        if (!resolveBreakpoint(script, debuggerBreakpoint))
            continue;

        if (!setBreakpoint(debuggerBreakpoint))
            continue;

        didSetBreakpoint(protocolBreakpoint, debuggerBreakpoint);

        m_frontendDispatcher->breakpointResolved(protocolBreakpoint.id(), buildDebuggerLocation(debuggerBreakpoint));
    }
}

void InspectorDebuggerAgent::failedToParseSource(const String& url, const String& data, int firstLine, int errorLine, const String& errorMessage)
{
    m_frontendDispatcher->scriptFailedToParse(url, data, firstLine, errorLine, errorMessage);
}

void InspectorDebuggerAgent::willRunMicrotask()
{
    if (!breakpointsActive())
        return;

    if (m_pauseOnMicrotasks)
        schedulePauseAtNextOpportunity(DebuggerFrontendDispatcher::Reason::Microtask);
}

void InspectorDebuggerAgent::didRunMicrotask()
{
    if (!breakpointsActive())
        return;

    if (m_pauseOnMicrotasks)
        cancelPauseAtNextOpportunity();
}

void InspectorDebuggerAgent::didPause(JSC::JSGlobalObject* globalObject, JSC::DebuggerCallFrame& debuggerCallFrame, JSC::JSValue exceptionOrCaughtValue)
{
    ASSERT(!m_pausedGlobalObject);
    m_pausedGlobalObject = globalObject;

    auto* debuggerGlobalObject = debuggerCallFrame.scope()->globalObject();
    m_currentCallStack = { m_pausedGlobalObject->vm(), toJS(debuggerGlobalObject, debuggerGlobalObject, JavaScriptCallFrame::create(debuggerCallFrame).ptr()) };

    InjectedScript injectedScript = m_injectedScriptManager.injectedScriptFor(m_pausedGlobalObject);

    // If a high level pause pause reason is not already set, try to infer a reason from the debugger.
    if (m_pauseReason == DebuggerFrontendDispatcher::Reason::Other) {
        switch (m_scriptDebugServer.reasonForPause()) {
        case JSC::Debugger::PausedForBreakpoint: {
            auto debuggerBreakpointId = m_scriptDebugServer.pausingBreakpointID();
            if (!m_continueToLocationDebuggerBreakpoint || debuggerBreakpointId != m_continueToLocationDebuggerBreakpoint->id())
                updatePauseReasonAndData(DebuggerFrontendDispatcher::Reason::Breakpoint, buildBreakpointPauseReason(debuggerBreakpointId));
            break;
        }
        case JSC::Debugger::PausedForDebuggerStatement:
            updatePauseReasonAndData(DebuggerFrontendDispatcher::Reason::DebuggerStatement, nullptr);
            break;
        case JSC::Debugger::PausedForException:
            updatePauseReasonAndData(DebuggerFrontendDispatcher::Reason::Exception, buildExceptionPauseReason(exceptionOrCaughtValue, injectedScript));
            break;
        case JSC::Debugger::PausedAfterBlackboxedScript: {
            // There should be no break data, as we would've already continued past the breakpoint.
            ASSERT(!m_pauseData);

            // Don't call `updatePauseReasonAndData` so as to not override `m_preBlackboxPauseData`.
            if (m_pauseReason != DebuggerFrontendDispatcher::Reason::BlackboxedScript)
                m_preBlackboxPauseReason = m_pauseReason;
            m_pauseReason = DebuggerFrontendDispatcher::Reason::BlackboxedScript;
            break;
        }
        case JSC::Debugger::PausedAtStatement:
        case JSC::Debugger::PausedAtExpression:
        case JSC::Debugger::PausedBeforeReturn:
        case JSC::Debugger::PausedAtEndOfProgram:
            // Pause was just stepping. Nothing to report.
            break;
        case JSC::Debugger::NotPaused:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    if (m_scriptDebugServer.reasonForPause() == JSC::Debugger::PausedAfterBlackboxedScript) {
        // Ensure that `m_preBlackboxPauseReason` is populated with the most recent data.
        updatePauseReasonAndData(m_pauseReason, nullptr);

        RefPtr<JSON::Object> data;
        if (auto debuggerBreakpointId = m_scriptDebugServer.pausingBreakpointID()) {
            ASSERT(!m_continueToLocationDebuggerBreakpoint || debuggerBreakpointId != m_continueToLocationDebuggerBreakpoint->id());
            data = JSON::Object::create();
            data->setString("originalReason"_s, Protocol::InspectorHelpers::getEnumConstantValue(DebuggerFrontendDispatcher::Reason::Breakpoint));
            data->setValue("originalData"_s, buildBreakpointPauseReason(debuggerBreakpointId));
        } else if (m_preBlackboxPauseData) {
            data = JSON::Object::create();
            data->setString("originalReason"_s, Protocol::InspectorHelpers::getEnumConstantValue(m_preBlackboxPauseReason));
            data->setValue("originalData"_s, m_preBlackboxPauseData);
        }
        updatePauseReasonAndData(DebuggerFrontendDispatcher::Reason::BlackboxedScript, WTFMove(data));
    }

    // Set $exception to the exception or caught value.
    if (exceptionOrCaughtValue && !injectedScript.hasNoValue()) {
        injectedScript.setExceptionValue(exceptionOrCaughtValue);
        m_hasExceptionValue = true;
    }

    m_conditionToDispatchResumed = ShouldDispatchResumed::No;
    m_enablePauseWhenIdle = false;

    RefPtr<Protocol::Console::StackTrace> asyncStackTrace;
    if (m_currentAsyncCallIdentifier) {
        auto it = m_pendingAsyncCalls.find(m_currentAsyncCallIdentifier.value());
        if (it != m_pendingAsyncCalls.end())
            asyncStackTrace = it->value->buildInspectorObject();
    }

    m_frontendDispatcher->paused(currentCallFrames(injectedScript), m_pauseReason, m_pauseData, asyncStackTrace);

    m_javaScriptPauseScheduled = false;

    if (m_continueToLocationDebuggerBreakpoint) {
        m_scriptDebugServer.removeBreakpoint(*m_continueToLocationDebuggerBreakpoint);
        m_continueToLocationDebuggerBreakpoint = nullptr;
    }

    auto& stopwatch = m_injectedScriptManager.inspectorEnvironment().executionStopwatch();
    if (stopwatch.isActive()) {
        stopwatch.stop();
        m_didPauseStopwatch = true;
    }
}

void InspectorDebuggerAgent::breakpointActionSound(JSC::BreakpointActionID id)
{
    m_frontendDispatcher->playBreakpointActionSound(id);
}

void InspectorDebuggerAgent::breakpointActionProbe(JSC::JSGlobalObject* globalObject, JSC::BreakpointActionID actionID, unsigned batchId, unsigned sampleId, JSC::JSValue sample)
{
    InjectedScript injectedScript = m_injectedScriptManager.injectedScriptFor(globalObject);
    auto payload = injectedScript.wrapObject(sample, objectGroupForBreakpointAction(actionID), true);
    auto result = Protocol::Debugger::ProbeSample::create()
        .setProbeId(actionID)
        .setBatchId(batchId)
        .setSampleId(sampleId)
        .setTimestamp(m_injectedScriptManager.inspectorEnvironment().executionStopwatch().elapsedTime().seconds())
        .setPayload(WTFMove(payload))
        .release();
    m_frontendDispatcher->didSampleProbe(WTFMove(result));
}

void InspectorDebuggerAgent::didContinue()
{
    if (m_didPauseStopwatch) {
        m_didPauseStopwatch = false;
        m_injectedScriptManager.inspectorEnvironment().executionStopwatch().start();
    }

    m_pausedGlobalObject = nullptr;
    m_currentCallStack = { };
    m_injectedScriptManager.releaseObjectGroup(InspectorDebuggerAgent::backtraceObjectGroup);
    clearPauseDetails();
    clearExceptionValue();

    if (m_conditionToDispatchResumed == ShouldDispatchResumed::WhenContinued)
        m_frontendDispatcher->resumed();
}

void InspectorDebuggerAgent::breakProgram(DebuggerFrontendDispatcher::Reason reason, RefPtr<JSON::Object>&& data)
{
    updatePauseReasonAndData(reason, WTFMove(data));

    m_scriptDebugServer.breakProgram();
}

void InspectorDebuggerAgent::clearInspectorBreakpointState()
{
    ErrorString ignored;
    for (auto& protocolBreakpointID : copyToVector(m_debuggerBreakpointsForProtocolBreakpointID.keys()))
        removeBreakpoint(ignored, protocolBreakpointID);

    m_protocolBreakpointForProtocolBreakpointID.clear();

    if (m_continueToLocationDebuggerBreakpoint) {
        m_scriptDebugServer.removeBreakpoint(*m_continueToLocationDebuggerBreakpoint);
        m_continueToLocationDebuggerBreakpoint = nullptr;
    }

    clearDebuggerBreakpointState();
}

void InspectorDebuggerAgent::clearDebuggerBreakpointState()
{
    {
        JSC::JSLockHolder holder(m_scriptDebugServer.vm());
        m_scriptDebugServer.clearBreakpoints();
        m_scriptDebugServer.clearBlackbox();
    }

    m_pausedGlobalObject = nullptr;
    m_currentCallStack = { };
    m_scripts.clear();
    m_protocolBreakpointForProtocolBreakpointID.clear();
    m_debuggerBreakpointsForProtocolBreakpointID.clear();
    m_nextDebuggerBreakpointID = JSC::noBreakpointID + 1;
    m_continueToLocationDebuggerBreakpoint = nullptr;
    clearPauseDetails();
    m_javaScriptPauseScheduled = false;
    m_hasExceptionValue = false;

    if (isPaused()) {
        m_scriptDebugServer.continueProgram();
        m_frontendDispatcher->resumed();
    }
}

void InspectorDebuggerAgent::didClearGlobalObject()
{
    // Clear breakpoints from the debugger, but keep the inspector's model of which
    // pages have what breakpoints, as the mapping is only sent to DebuggerAgent once.
    clearDebuggerBreakpointState();

    clearAsyncStackTraceData();

    m_frontendDispatcher->globalObjectCleared();
}

bool InspectorDebuggerAgent::assertPaused(ErrorString& errorString)
{
    if (!m_pausedGlobalObject) {
        errorString = "Must be paused"_s;
        return false;
    }

    return true;
}

void InspectorDebuggerAgent::clearPauseDetails()
{
    updatePauseReasonAndData(DebuggerFrontendDispatcher::Reason::Other, nullptr);
}

void InspectorDebuggerAgent::clearExceptionValue()
{
    if (m_hasExceptionValue) {
        m_injectedScriptManager.clearExceptionValue();
        m_hasExceptionValue = false;
    }
}

void InspectorDebuggerAgent::clearAsyncStackTraceData()
{
    m_pendingAsyncCalls.clear();
    m_currentAsyncCallIdentifier = WTF::nullopt;

    didClearAsyncStackTraceData();
}

} // namespace Inspector
