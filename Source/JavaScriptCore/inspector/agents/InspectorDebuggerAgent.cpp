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

static Optional<JSC::Breakpoint::Action::Type> breakpointActionTypeForString(Protocol::ErrorString& errorString, const String& typeString)
{
    auto type = Protocol::Helpers::parseEnumValueFromString<Protocol::Debugger::BreakpointAction::Type>(typeString);
    if (!type) {
        errorString = makeString("Unknown breakpoint action type: "_s, typeString);
        return WTF::nullopt;
    }

    switch (*type) {
    case Protocol::Debugger::BreakpointAction::Type::Log:
        return JSC::Breakpoint::Action::Type::Log;

    case Protocol::Debugger::BreakpointAction::Type::Evaluate:
        return JSC::Breakpoint::Action::Type::Evaluate;

    case Protocol::Debugger::BreakpointAction::Type::Sound:
        return JSC::Breakpoint::Action::Type::Sound;

    case Protocol::Debugger::BreakpointAction::Type::Probe:
        return JSC::Breakpoint::Action::Type::Probe;
    }

    ASSERT_NOT_REACHED();
    return WTF::nullopt;
}

template <typename T>
static T parseBreakpointOptions(Protocol::ErrorString& errorString, RefPtr<JSON::Object>&& options, Function<T(const String&, JSC::Breakpoint::ActionsVector&&, bool, size_t)> callback)
{
    String condition;
    JSC::Breakpoint::ActionsVector actions;
    bool autoContinue = false;
    size_t ignoreCount = 0;

    if (options) {
        condition = options->getString(Protocol::Debugger::BreakpointOptions::conditionKey);

        auto actionsPayload = options->getArray(Protocol::Debugger::BreakpointOptions::actionsKey);
        if (auto count = actionsPayload ? actionsPayload->length() : 0) {
            actions.reserveCapacity(count);

            for (unsigned i = 0; i < count; ++i) {
                auto actionObject = actionsPayload->get(i)->asObject();
                if (!actionObject) {
                    errorString = "Unexpected non-object item in given actions"_s;
                    return { };
                }

                auto actionTypeString = actionObject->getString(Protocol::Debugger::BreakpointAction::typeKey);
                if (!actionTypeString) {
                    errorString = "Missing type for item in given actions"_s;
                    return { };
                }

                auto actionType = breakpointActionTypeForString(errorString, actionTypeString);
                if (!actionType)
                    return { };

                JSC::Breakpoint::Action action(*actionType);

                action.data = actionObject->getString(Protocol::Debugger::BreakpointAction::dataKey);

                // Specifying an identifier is optional. They are used to correlate probe samples
                // in the frontend across multiple backend probe actions and segregate object groups.
                action.id = actionObject->getInteger(Protocol::Debugger::BreakpointAction::idKey).valueOr(JSC::noBreakpointActionID);

                actions.append(WTFMove(action));
            }
        }

        autoContinue = options->getBoolean(Protocol::Debugger::BreakpointOptions::autoContinueKey).valueOr(false);
        ignoreCount = options->getInteger(Protocol::Debugger::BreakpointOptions::ignoreCountKey).valueOr(0);
    }

    return callback(condition, WTFMove(actions), autoContinue, ignoreCount);
}

Optional<InspectorDebuggerAgent::ProtocolBreakpoint> InspectorDebuggerAgent::ProtocolBreakpoint::fromPayload(Protocol::ErrorString& errorString, JSC::SourceID sourceID, unsigned lineNumber, unsigned columnNumber, RefPtr<JSON::Object>&& options)
{
    return parseBreakpointOptions<Optional<ProtocolBreakpoint>>(errorString, WTFMove(options), [&] (const String& condition, JSC::Breakpoint::ActionsVector&& actions, bool autoContinue, size_t ignoreCount) -> Optional<ProtocolBreakpoint> {
        return ProtocolBreakpoint(sourceID, lineNumber, columnNumber, condition, WTFMove(actions), autoContinue, ignoreCount);
    });
}

Optional<InspectorDebuggerAgent::ProtocolBreakpoint> InspectorDebuggerAgent::ProtocolBreakpoint::fromPayload(Protocol::ErrorString& errorString, const String& url, bool isRegex, unsigned lineNumber, unsigned columnNumber, RefPtr<JSON::Object>&& options)
{
    return parseBreakpointOptions<Optional<ProtocolBreakpoint>>(errorString, WTFMove(options), [&] (const String& condition, JSC::Breakpoint::ActionsVector&& actions, bool autoContinue, size_t ignoreCount) -> Optional<ProtocolBreakpoint> {
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

RefPtr<JSC::Breakpoint> InspectorDebuggerAgent::debuggerBreakpointFromPayload(Protocol::ErrorString& errorString, RefPtr<JSON::Object>&& options)
{
    return parseBreakpointOptions<RefPtr<JSC::Breakpoint>>(errorString, WTFMove(options), [] (const String& condition, JSC::Breakpoint::ActionsVector&& actions, bool autoContinue, size_t ignoreCount) {
        return JSC::Breakpoint::create(JSC::noBreakpointID, condition, WTFMove(actions), autoContinue, ignoreCount);
    });
}

InspectorDebuggerAgent::InspectorDebuggerAgent(AgentContext& context)
    : InspectorAgentBase("Debugger"_s)
    , m_frontendDispatcher(makeUnique<DebuggerFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(DebuggerBackendDispatcher::create(context.backendDispatcher, this))
    , m_debugger(context.environment.debugger())
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
        internalDisable(reason == DisconnectReason::InspectedTargetDestroyed);
}

void InspectorDebuggerAgent::internalEnable()
{
    m_enabled = true;

    m_debugger.addObserver(*this);

    for (auto* listener : copyToVector(m_listeners))
        listener->debuggerWasEnabled();

    for (auto& [sourceID, script] : m_scripts) {
        Optional<JSC::Debugger::BlackboxType> blackboxType;
        if (isWebKitInjectedScript(script.sourceURL)) {
            if (!m_pauseForInternalScripts)
                blackboxType = JSC::Debugger::BlackboxType::Ignored;
        } else if (shouldBlackboxURL(script.sourceURL) || shouldBlackboxURL(script.url))
            blackboxType = JSC::Debugger::BlackboxType::Deferred;
        m_debugger.setBlackboxType(sourceID, blackboxType);
    }
}

void InspectorDebuggerAgent::internalDisable(bool isBeingDestroyed)
{
    for (auto* listener : copyToVector(m_listeners))
        listener->debuggerWasDisabled();

    m_debugger.removeObserver(*this, isBeingDestroyed);

    clearInspectorBreakpointState();

    if (!isBeingDestroyed)
        m_debugger.deactivateBreakpoints();

    clearAsyncStackTraceData();

    m_pauseOnAssertionsBreakpoint = nullptr;
    m_pauseOnMicrotasksBreakpoint = nullptr;

    m_enabled = false;
}

Protocol::ErrorStringOr<void> InspectorDebuggerAgent::enable()
{
    if (enabled())
        return makeUnexpected("Debugger domain already enabled"_s);

    internalEnable();

    return { };
}

Protocol::ErrorStringOr<void> InspectorDebuggerAgent::disable()
{
    internalDisable(false);

    return { };
}

bool InspectorDebuggerAgent::breakpointsActive() const
{
    return m_debugger.breakpointsActive();
}

Protocol::ErrorStringOr<void> InspectorDebuggerAgent::setAsyncStackTraceDepth(int depth)
{
    if (m_asyncStackTraceDepth == depth)
        return { };

    if (depth < 0)
        return makeUnexpected("Unexpected negative depth"_s);

    m_asyncStackTraceDepth = depth;

    if (!m_asyncStackTraceDepth)
        clearAsyncStackTraceData();

    return { };
}

Protocol::ErrorStringOr<void> InspectorDebuggerAgent::setBreakpointsActive(bool active)
{
    if (active)
        m_debugger.activateBreakpoints();
    else
        m_debugger.deactivateBreakpoints();

    return { };
}

bool InspectorDebuggerAgent::isPaused() const
{
    return m_debugger.isPaused();
}

void InspectorDebuggerAgent::setSuppressAllPauses(bool suppress)
{
    m_debugger.setSuppressAllPauses(suppress);
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

static Ref<JSON::Object> buildAssertPauseReason(const String& message)
{
    auto reason = Protocol::Debugger::AssertPauseReason::create().release();
    if (!message.isNull())
        reason->setMessage(message);
    return *reason->asObject();
}

static Ref<JSON::Object> buildCSPViolationPauseReason(const String& directiveText)
{
    auto reason = Protocol::Debugger::CSPViolationPauseReason::create()
        .setDirective(directiveText)
        .release();
    return *reason->asObject();
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
                return reason->asObject();
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

    auto exceptionValue = injectedScript.wrapObject(exception, InspectorDebuggerAgent::backtraceObjectGroup);
    if (!exceptionValue)
        return nullptr;

    return exceptionValue->asObject();
}

void InspectorDebuggerAgent::handleConsoleAssert(const String& message)
{
    if (!breakpointsActive())
        return;

    if (!m_pauseOnAssertionsBreakpoint)
        return;

    breakProgram(DebuggerFrontendDispatcher::Reason::Assert, buildAssertPauseReason(message), m_pauseOnAssertionsBreakpoint.copyRef());
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

static Ref<Protocol::Debugger::Location> buildDebuggerLocation(const JSC::Breakpoint& debuggerBreakpoint)
{
    ASSERT(debuggerBreakpoint.isResolved());

    auto location = Protocol::Debugger::Location::create()
        .setScriptId(String::number(debuggerBreakpoint.sourceID()))
        .setLineNumber(debuggerBreakpoint.lineNumber())
        .release();
    location->setColumnNumber(debuggerBreakpoint.columnNumber());
    return location;
}

static bool parseLocation(Protocol::ErrorString& errorString, const JSON::Object& location, JSC::SourceID& sourceID, unsigned& lineNumber, unsigned& columnNumber)
{
    auto lineNumberValue = location.getInteger(Protocol::Debugger::Location::lineNumberKey);
    if (!lineNumberValue) {
        errorString = "Unexpected non-integer lineNumber in given location"_s;
        sourceID = JSC::noSourceID;
        return false;
    }

    lineNumber = *lineNumberValue;

    auto scriptIDStr = location.getString(Protocol::Debugger::Location::scriptIdKey);
    if (!scriptIDStr) {
        sourceID = JSC::noSourceID;
        errorString = "Unexepcted non-string scriptId in given location"_s;
        return false;
    }

    sourceID = scriptIDStr.toIntPtr();
    columnNumber = location.getInteger(Protocol::Debugger::Location::columnNumberKey).valueOr(0);
    return true;
}

Protocol::ErrorStringOr<std::tuple<Protocol::Debugger::BreakpointId, Ref<JSON::ArrayOf<Protocol::Debugger::Location>>>> InspectorDebuggerAgent::setBreakpointByUrl(int lineNumber, const String& url, const String& urlRegex, Optional<int>&& columnNumber, RefPtr<JSON::Object>&& options)
{
    if (!url == !urlRegex)
        return makeUnexpected("Either url or urlRegex must be specified"_s);

    Protocol::ErrorString errorString;

    auto protocolBreakpoint = ProtocolBreakpoint::fromPayload(errorString, !!url ? url : urlRegex, !!urlRegex, lineNumber, columnNumber.valueOr(0), WTFMove(options));
    if (!protocolBreakpoint)
        return makeUnexpected(errorString);

    if (m_protocolBreakpointForProtocolBreakpointID.contains(protocolBreakpoint->id()))
        return makeUnexpected("Breakpoint for given location already exists."_s);

    m_protocolBreakpointForProtocolBreakpointID.set(protocolBreakpoint->id(), *protocolBreakpoint);

    auto locations = JSON::ArrayOf<Protocol::Debugger::Location>::create();

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

        locations->addItem(buildDebuggerLocation(debuggerBreakpoint));
    }

    return { { protocolBreakpoint->id(), WTFMove(locations) } };
}

Protocol::ErrorStringOr<std::tuple<Protocol::Debugger::BreakpointId, Ref<Protocol::Debugger::Location>>> InspectorDebuggerAgent::setBreakpoint(Ref<JSON::Object>&& location, RefPtr<JSON::Object>&& options)
{
    Protocol::ErrorString errorString;

    JSC::SourceID sourceID;
    unsigned lineNumber;
    unsigned columnNumber;
    if (!parseLocation(errorString, location, sourceID, lineNumber, columnNumber))
        return makeUnexpected(errorString);

    auto scriptIterator = m_scripts.find(sourceID);
    if (scriptIterator == m_scripts.end())
        return makeUnexpected("Missing script for scriptId in given location"_s);

    auto protocolBreakpoint = ProtocolBreakpoint::fromPayload(errorString, sourceID, lineNumber, columnNumber, WTFMove(options));
    if (!protocolBreakpoint)
        return makeUnexpected(errorString);

    // Don't save `protocolBreakpoint` in `m_protocolBreakpointForProtocolBreakpointID` because it
    // was set specifically for the given `sourceID`, which is unique, meaning that it will never
    // be used inside `InspectorDebuggerAgent::didParseSource`.

    auto debuggerBreakpoint = protocolBreakpoint->createDebuggerBreakpoint(m_nextDebuggerBreakpointID++, sourceID);

    if (!resolveBreakpoint(scriptIterator->value, debuggerBreakpoint))
        return makeUnexpected("Could not resolve breakpoint"_s);

    if (!setBreakpoint(debuggerBreakpoint))
        return makeUnexpected("Breakpoint for given location already exists"_s);

    didSetBreakpoint(*protocolBreakpoint, debuggerBreakpoint);

    return { { protocolBreakpoint->id(), buildDebuggerLocation(debuggerBreakpoint) } };
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

    return m_debugger.resolveBreakpoint(debuggerBreakpoint, script.sourceProvider.get());
}

bool InspectorDebuggerAgent::setBreakpoint(JSC::Breakpoint& debuggerBreakpoint)
{
    JSC::JSLockHolder locker(m_debugger.vm());
    return m_debugger.setBreakpoint(debuggerBreakpoint);
}

Protocol::ErrorStringOr<void> InspectorDebuggerAgent::removeBreakpoint(const Protocol::Debugger::BreakpointId& protocolBreakpointID)
{
    m_protocolBreakpointForProtocolBreakpointID.remove(protocolBreakpointID);

    for (auto& debuggerBreakpoint : m_debuggerBreakpointsForProtocolBreakpointID.take(protocolBreakpointID)) {
        for (const auto& action : debuggerBreakpoint->actions())
            m_injectedScriptManager.releaseObjectGroup(objectGroupForBreakpointAction(action.id));

        JSC::JSLockHolder locker(m_debugger.vm());
        m_debugger.removeBreakpoint(debuggerBreakpoint);
    }

    return { };
}

Protocol::ErrorStringOr<void> InspectorDebuggerAgent::continueUntilNextRunLoop()
{
    Protocol::ErrorString errorString;

    if (!assertPaused(errorString))
        return makeUnexpected(errorString);

    auto resumeResult = resume();
    if (!resumeResult)
        return makeUnexpected(resumeResult.error());

    m_enablePauseWhenIdle = true;

    registerIdleHandler();

    return { };
}

Protocol::ErrorStringOr<void> InspectorDebuggerAgent::continueToLocation(Ref<JSON::Object>&& location)
{
    Protocol::ErrorString errorString;

    if (!assertPaused(errorString))
        return makeUnexpected(errorString);

    if (m_continueToLocationDebuggerBreakpoint) {
        m_debugger.removeBreakpoint(*m_continueToLocationDebuggerBreakpoint);
        m_continueToLocationDebuggerBreakpoint = nullptr;
    }

    JSC::SourceID sourceID;
    unsigned lineNumber;
    unsigned columnNumber;
    if (!parseLocation(errorString, location, sourceID, lineNumber, columnNumber))
        return makeUnexpected(errorString);

    auto scriptIterator = m_scripts.find(sourceID);
    if (scriptIterator == m_scripts.end()) {
        m_debugger.continueProgram();
        m_frontendDispatcher->resumed();
        return makeUnexpected("Missing script for scriptId in given location"_s);
    }

    auto protocolBreakpoint = ProtocolBreakpoint::fromPayload(errorString, sourceID, lineNumber, columnNumber);
    if (!protocolBreakpoint)
        return makeUnexpected(errorString);

    // Don't save `protocolBreakpoint` in `m_protocolBreakpointForProtocolBreakpointID` because it
    // is a temporary breakpoint that will be removed as soon as `location` is reached.

    auto debuggerBreakpoint = protocolBreakpoint->createDebuggerBreakpoint(m_nextDebuggerBreakpointID++, sourceID);

    if (!resolveBreakpoint(scriptIterator->value, debuggerBreakpoint)) {
        m_debugger.continueProgram();
        m_frontendDispatcher->resumed();
        return makeUnexpected("Could not resolve breakpoint"_s);
    }

    if (!setBreakpoint(debuggerBreakpoint)) {
        // There is an existing breakpoint at this location. Instead of
        // acting like a series of steps, just resume and we will either
        // hit this new breakpoint or not.
        m_debugger.continueProgram();
        m_frontendDispatcher->resumed();
        return { };
    }

    m_continueToLocationDebuggerBreakpoint = WTFMove(debuggerBreakpoint);

    // Treat this as a series of steps until reaching the new breakpoint.
    // So don't issue a resumed event unless we exit the VM without pausing.
    willStepAndMayBecomeIdle();
    m_debugger.continueProgram();

    return { };
}

Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Protocol::GenericTypes::SearchMatch>>> InspectorDebuggerAgent::searchInContent(const Protocol::Debugger::ScriptId& scriptId, const String& query, Optional<bool>&& caseSensitive, Optional<bool>&& isRegex)
{
    auto it = m_scripts.find(scriptId.toIntPtr());
    if (it == m_scripts.end())
        return makeUnexpected("Missing script for given scriptId"_s);

    return ContentSearchUtilities::searchInTextByLines(it->value.source, query, caseSensitive.valueOr(false), isRegex.valueOr(false));
}

Protocol::ErrorStringOr<String> InspectorDebuggerAgent::getScriptSource(const Protocol::Debugger::ScriptId& scriptId)
{
    auto it = m_scripts.find(scriptId.toIntPtr());
    if (it == m_scripts.end())
        return makeUnexpected("Missing script for given scriptId");

    return it->value.source;
}

Protocol::ErrorStringOr<Ref<Protocol::Debugger::FunctionDetails>> InspectorDebuggerAgent::getFunctionDetails(const String& functionId)
{
    Protocol::ErrorString errorString;

    InjectedScript injectedScript = m_injectedScriptManager.injectedScriptForObjectId(functionId);
    if (injectedScript.hasNoValue())
        return makeUnexpected("Missing injected script for given functionId"_s);

    RefPtr<Protocol::Debugger::FunctionDetails> details;
    injectedScript.getFunctionDetails(errorString, functionId, details);
    if (!details)
        return makeUnexpected(errorString);

    return details.releaseNonNull();
}

void InspectorDebuggerAgent::schedulePauseAtNextOpportunity(DebuggerFrontendDispatcher::Reason reason, RefPtr<JSON::Object>&& data)
{
    if (m_javaScriptPauseScheduled)
        return;

    m_javaScriptPauseScheduled = true;

    updatePauseReasonAndData(reason, WTFMove(data));

    JSC::JSLockHolder locker(m_debugger.vm());
    m_debugger.schedulePauseAtNextOpportunity();
}

void InspectorDebuggerAgent::cancelPauseAtNextOpportunity()
{
    if (!m_javaScriptPauseScheduled)
        return;

    m_javaScriptPauseScheduled = false;

    clearPauseDetails();
    m_debugger.cancelPauseAtNextOpportunity();
    m_enablePauseWhenIdle = false;
}

bool InspectorDebuggerAgent::schedulePauseForSpecialBreakpoint(JSC::Breakpoint& breakpoint, DebuggerFrontendDispatcher::Reason reason, RefPtr<JSON::Object>&& data)
{
    JSC::JSLockHolder locker(m_debugger.vm());

    if (!m_debugger.schedulePauseForSpecialBreakpoint(breakpoint))
        return false;

    updatePauseReasonAndData(reason, WTFMove(data));
    return true;
}

bool InspectorDebuggerAgent::cancelPauseForSpecialBreakpoint(JSC::Breakpoint& breakpoint)
{
    if (!m_debugger.cancelPauseForSpecialBreakpoint(breakpoint))
        return false;

    clearPauseDetails();
    return true;
}

Protocol::ErrorStringOr<void> InspectorDebuggerAgent::pause()
{
    schedulePauseAtNextOpportunity(DebuggerFrontendDispatcher::Reason::PauseOnNextStatement);

    return { };
}

Protocol::ErrorStringOr<void> InspectorDebuggerAgent::resume()
{
    if (!m_pausedGlobalObject && !m_javaScriptPauseScheduled)
        return makeUnexpected("Must be paused or waiting to pause"_s);

    cancelPauseAtNextOpportunity();
    m_debugger.continueProgram();
    m_conditionToDispatchResumed = ShouldDispatchResumed::WhenContinued;

    return { };
}

Protocol::ErrorStringOr<void> InspectorDebuggerAgent::stepNext()
{
    Protocol::ErrorString errorString;

    if (!assertPaused(errorString))
        return makeUnexpected(errorString);

    willStepAndMayBecomeIdle();
    m_debugger.stepNextExpression();

    return { };
}

Protocol::ErrorStringOr<void> InspectorDebuggerAgent::stepOver()
{
    Protocol::ErrorString errorString;

    if (!assertPaused(errorString))
        return makeUnexpected(errorString);

    willStepAndMayBecomeIdle();
    m_debugger.stepOverStatement();

    return { };
}

Protocol::ErrorStringOr<void> InspectorDebuggerAgent::stepInto()
{
    Protocol::ErrorString errorString;

    if (!assertPaused(errorString))
        return makeUnexpected(errorString);

    willStepAndMayBecomeIdle();
    m_debugger.stepIntoStatement();

    return { };
}

Protocol::ErrorStringOr<void> InspectorDebuggerAgent::stepOut()
{
    Protocol::ErrorString errorString;

    if (!assertPaused(errorString))
        return makeUnexpected(errorString);

    willStepAndMayBecomeIdle();
    m_debugger.stepOutOfFunction();

    return { };
}

void InspectorDebuggerAgent::registerIdleHandler()
{
    if (!m_registeredIdleCallback) {
        m_registeredIdleCallback = true;
        JSC::VM& vm = m_debugger.vm();
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
        m_debugger.continueProgram();
        m_frontendDispatcher->resumed();
    }

    m_conditionToDispatchResumed = ShouldDispatchResumed::No;

    if (m_enablePauseWhenIdle)
        pause();
}

Protocol::ErrorStringOr<void> InspectorDebuggerAgent::setPauseOnDebuggerStatements(bool enabled, RefPtr<JSON::Object>&& options)
{
    Protocol::ErrorString errorString;

    if (!enabled) {
        m_debugger.setPauseOnDebuggerStatementsBreakpoint(nullptr);
        return { };
    }

    auto breakpoint = debuggerBreakpointFromPayload(errorString, WTFMove(options));
    if (!breakpoint)
        return makeUnexpected(errorString);

    m_debugger.setPauseOnDebuggerStatementsBreakpoint(WTFMove(breakpoint));

    return { };
}

Protocol::ErrorStringOr<void> InspectorDebuggerAgent::setPauseOnExceptions(const String& stateString, RefPtr<JSON::Object>&& options)
{
    Protocol::ErrorString errorString;

    RefPtr<JSC::Breakpoint> allExceptionsBreakpoint;
    RefPtr<JSC::Breakpoint> uncaughtExceptionsBreakpoint;

    if (stateString == "all") {
        allExceptionsBreakpoint = debuggerBreakpointFromPayload(errorString, WTFMove(options));
        if (!allExceptionsBreakpoint)
            return makeUnexpected(errorString);
    } else if (stateString == "uncaught") {
        uncaughtExceptionsBreakpoint = debuggerBreakpointFromPayload(errorString, WTFMove(options));
        if (!uncaughtExceptionsBreakpoint)
            return makeUnexpected(errorString);
    } else if (stateString != "none")
        return makeUnexpected(makeString("Unknown state: "_s, stateString));

    m_debugger.setPauseOnAllExceptionsBreakpoint(WTFMove(allExceptionsBreakpoint));
    m_debugger.setPauseOnUncaughtExceptionsBreakpoint(WTFMove(uncaughtExceptionsBreakpoint));

    return { };
}

Protocol::ErrorStringOr<void> InspectorDebuggerAgent::setPauseOnAssertions(bool enabled, RefPtr<JSON::Object>&& options)
{
    Protocol::ErrorString errorString;

    if (!enabled) {
        m_pauseOnAssertionsBreakpoint = nullptr;
        return { };
    }

    auto breakpoint = debuggerBreakpointFromPayload(errorString, WTFMove(options));
    if (!breakpoint)
        return makeUnexpected(errorString);

    m_pauseOnAssertionsBreakpoint = WTFMove(breakpoint);

    return { };
}

Protocol::ErrorStringOr<void> InspectorDebuggerAgent::setPauseOnMicrotasks(bool enabled, RefPtr<JSON::Object>&& options)
{
    Protocol::ErrorString errorString;

    if (!enabled) {
        m_pauseOnMicrotasksBreakpoint = nullptr;
        return { };
    }

    auto breakpoint = debuggerBreakpointFromPayload(errorString, WTFMove(options));
    if (!breakpoint)
        return makeUnexpected(errorString);

    m_pauseOnMicrotasksBreakpoint = WTFMove(breakpoint);

    return { };
}

Protocol::ErrorStringOr<std::tuple<Ref<Protocol::Runtime::RemoteObject>, Optional<bool> /* wasThrown */, Optional<int> /* savedResultIndex */>> InspectorDebuggerAgent::evaluateOnCallFrame(const Protocol::Debugger::CallFrameId& callFrameId, const String& expression, const String& objectGroup, Optional<bool>&& includeCommandLineAPI, Optional<bool>&& doNotPauseOnExceptionsAndMuteConsole, Optional<bool>&& returnByValue, Optional<bool>&& generatePreview, Optional<bool>&& saveResult, Optional<bool>&&  /* emulateUserGesture */)
{
    Protocol::ErrorString errorString;

    if (!assertPaused(errorString))
        return makeUnexpected(errorString);

    InjectedScript injectedScript = m_injectedScriptManager.injectedScriptForObjectId(callFrameId);
    if (injectedScript.hasNoValue())
        return makeUnexpected("Missing injected script for given callFrameId"_s);

    JSC::Debugger::TemporarilyDisableExceptionBreakpoints temporarilyDisableExceptionBreakpoints(m_debugger);

    bool pauseAndMute = doNotPauseOnExceptionsAndMuteConsole && *doNotPauseOnExceptionsAndMuteConsole;
    if (pauseAndMute) {
        temporarilyDisableExceptionBreakpoints.replace();
        muteConsole();
    }

    RefPtr<Protocol::Runtime::RemoteObject> result;
    Optional<bool> wasThrown;
    Optional<int> savedResultIndex;

    injectedScript.evaluateOnCallFrame(errorString, m_currentCallStack.get(), callFrameId, expression, objectGroup, includeCommandLineAPI.valueOr(false), returnByValue.valueOr(false), generatePreview.valueOr(false), saveResult.valueOr(false), result, wasThrown, savedResultIndex);

    if (pauseAndMute)
        unmuteConsole();

    if (!result)
        return makeUnexpected(errorString);

    return { { result.releaseNonNull(), WTFMove(wasThrown), WTFMove(savedResultIndex) } };
}

Protocol::ErrorStringOr<void> InspectorDebuggerAgent::setShouldBlackboxURL(const String& url, bool shouldBlackbox, Optional<bool>&& optionalCaseSensitive, Optional<bool>&& optionalIsRegex)
{
    if (url.isEmpty())
        return makeUnexpected("URL must not be empty"_s);

    bool caseSensitive = optionalCaseSensitive.valueOr(false);
    bool isRegex = optionalIsRegex.valueOr(false);

    if (!caseSensitive && !isRegex && isWebKitInjectedScript(url))
        return makeUnexpected("Blackboxing of internal scripts is controlled by 'Debugger.setPauseForInternalScripts'"_s);

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
        m_debugger.setBlackboxType(sourceID, blackboxType);
    }

    return { };
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
    if (m_debugger.needsExceptionCallbacks())
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

Protocol::ErrorStringOr<void> InspectorDebuggerAgent::setPauseForInternalScripts(bool shouldPause)
{
    if (shouldPause == m_pauseForInternalScripts)
        return { };

    m_pauseForInternalScripts = shouldPause;

    auto blackboxType = !m_pauseForInternalScripts ? Optional<JSC::Debugger::BlackboxType>(JSC::Debugger::BlackboxType::Ignored) : WTF::nullopt;
    for (auto& [sourceID, script] : m_scripts) {
        if (!isWebKitInjectedScript(script.sourceURL))
            continue;
        m_debugger.setBlackboxType(sourceID, blackboxType);
    }

    return { };
}

void InspectorDebuggerAgent::didParseSource(JSC::SourceID sourceID, const JSC::Debugger::Script& script)
{
    String scriptIDStr = String::number(sourceID);
    bool hasSourceURL = !script.sourceURL.isEmpty();
    String sourceURL = script.sourceURL;
    String sourceMappingURL = sourceMapURLForScript(script);

    m_frontendDispatcher->scriptParsed(scriptIDStr, script.url, script.startLine, script.startColumn, script.endLine, script.endColumn, script.isContentScript, sourceURL, sourceMappingURL, script.sourceProvider->sourceType() == JSC::SourceProviderSourceType::Module);

    m_scripts.set(sourceID, script);

    if (isWebKitInjectedScript(sourceURL)) {
        if (!m_pauseForInternalScripts)
            m_debugger.setBlackboxType(sourceID, JSC::Debugger::BlackboxType::Ignored);
    } else if (shouldBlackboxURL(sourceURL) || shouldBlackboxURL(script.url))
        m_debugger.setBlackboxType(sourceID, JSC::Debugger::BlackboxType::Deferred);

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

    if (!m_pauseOnMicrotasksBreakpoint)
        return;

    schedulePauseForSpecialBreakpoint(*m_pauseOnMicrotasksBreakpoint, DebuggerFrontendDispatcher::Reason::Microtask);
}

void InspectorDebuggerAgent::didRunMicrotask()
{
    if (!breakpointsActive())
        return;

    if (!m_pauseOnMicrotasksBreakpoint)
        return;

    cancelPauseForSpecialBreakpoint(*m_pauseOnMicrotasksBreakpoint);
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
        switch (m_debugger.reasonForPause()) {
        case JSC::Debugger::PausedForBreakpoint: {
            auto debuggerBreakpointId = m_debugger.pausingBreakpointID();
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

    if (m_debugger.reasonForPause() == JSC::Debugger::PausedAfterBlackboxedScript) {
        // Ensure that `m_preBlackboxPauseReason` is populated with the most recent data.
        updatePauseReasonAndData(m_pauseReason, nullptr);

        RefPtr<JSON::Object> data;
        if (auto debuggerBreakpointId = m_debugger.pausingBreakpointID()) {
            ASSERT(!m_continueToLocationDebuggerBreakpoint || debuggerBreakpointId != m_continueToLocationDebuggerBreakpoint->id());
            data = JSON::Object::create();
            data->setString("originalReason"_s, Protocol::Helpers::getEnumConstantValue(DebuggerFrontendDispatcher::Reason::Breakpoint));
            if (auto pauseReason = buildBreakpointPauseReason(debuggerBreakpointId))
                data->setValue("originalData"_s, pauseReason.releaseNonNull());
        } else if (m_preBlackboxPauseData) {
            data = JSON::Object::create();
            data->setString("originalReason"_s, Protocol::Helpers::getEnumConstantValue(m_preBlackboxPauseReason));
            data->setValue("originalData"_s, m_preBlackboxPauseData.releaseNonNull());
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

    m_frontendDispatcher->paused(currentCallFrames(injectedScript), Protocol::Helpers::getEnumConstantValue(m_pauseReason), m_pauseData.copyRef(), WTFMove(asyncStackTrace));

    m_javaScriptPauseScheduled = false;

    if (m_continueToLocationDebuggerBreakpoint) {
        m_debugger.removeBreakpoint(*m_continueToLocationDebuggerBreakpoint);
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
    if (!payload)
        return;

    auto result = Protocol::Debugger::ProbeSample::create()
        .setProbeId(actionID)
        .setBatchId(batchId)
        .setSampleId(sampleId)
        .setTimestamp(m_injectedScriptManager.inspectorEnvironment().executionStopwatch().elapsedTime().seconds())
        .setPayload(payload.releaseNonNull())
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

void InspectorDebuggerAgent::breakProgram(DebuggerFrontendDispatcher::Reason reason, RefPtr<JSON::Object>&& data, RefPtr<JSC::Breakpoint>&& specialBreakpoint)
{
    updatePauseReasonAndData(reason, WTFMove(data));

    m_debugger.breakProgram(WTFMove(specialBreakpoint));
}

void InspectorDebuggerAgent::clearInspectorBreakpointState()
{
    for (auto& protocolBreakpointID : copyToVector(m_debuggerBreakpointsForProtocolBreakpointID.keys()))
        removeBreakpoint(protocolBreakpointID);

    m_protocolBreakpointForProtocolBreakpointID.clear();

    if (m_continueToLocationDebuggerBreakpoint) {
        m_debugger.removeBreakpoint(*m_continueToLocationDebuggerBreakpoint);
        m_continueToLocationDebuggerBreakpoint = nullptr;
    }

    clearDebuggerBreakpointState();
}

void InspectorDebuggerAgent::clearDebuggerBreakpointState()
{
    {
        JSC::JSLockHolder holder(m_debugger.vm());
        m_debugger.clearBreakpoints();
        m_debugger.clearBlackbox();
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
        m_debugger.continueProgram();
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

bool InspectorDebuggerAgent::assertPaused(Protocol::ErrorString& errorString)
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
