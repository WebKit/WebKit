/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "InspectorDOMDebuggerAgent.h"

#include "Event.h"
#include "EventTarget.h"
#include "InspectorDOMAgent.h"
#include "InstrumentingAgents.h"
#include "JSEvent.h"
#include "RegisteredEventListener.h"
#include "ScriptExecutionContext.h"
#include <JavaScriptCore/ContentSearchUtilities.h>
#include <JavaScriptCore/InjectedScript.h>
#include <JavaScriptCore/InjectedScriptManager.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <JavaScriptCore/RegularExpression.h>
#include <wtf/JSONValues.h>

namespace WebCore {

using namespace Inspector;

InspectorDOMDebuggerAgent::InspectorDOMDebuggerAgent(WebAgentContext& context, InspectorDebuggerAgent* debuggerAgent)
    : InspectorAgentBase("DOMDebugger"_s, context)
    , m_debuggerAgent(debuggerAgent)
    , m_backendDispatcher(Inspector::DOMDebuggerBackendDispatcher::create(context.backendDispatcher, this))
    , m_injectedScriptManager(context.injectedScriptManager)
{
    m_debuggerAgent->addListener(*this);
}

InspectorDOMDebuggerAgent::~InspectorDOMDebuggerAgent() = default;

bool InspectorDOMDebuggerAgent::enabled() const
{
    return m_instrumentingAgents.enabledDOMDebuggerAgent() == this;
}

void InspectorDOMDebuggerAgent::enable()
{
    m_instrumentingAgents.setEnabledDOMDebuggerAgent(this);
}

void InspectorDOMDebuggerAgent::disable()
{
    m_instrumentingAgents.setEnabledDOMDebuggerAgent(nullptr);

    m_listenerBreakpoints.clear();
    m_pauseOnAllIntervalsBreakpoint = nullptr;
    m_pauseOnAllListenersBreakpoint = nullptr;
    m_pauseOnAllTimeoutsBreakpoint = nullptr;

    m_urlTextBreakpoints.clear();
    m_urlRegexBreakpoints.clear();
    m_pauseOnAllURLsBreakpoint = nullptr;
}

// Browser debugger agent enabled only when JS debugger is enabled.
void InspectorDOMDebuggerAgent::debuggerWasEnabled()
{
    ASSERT(!enabled());
    enable();
}

void InspectorDOMDebuggerAgent::debuggerWasDisabled()
{
    ASSERT(enabled());
    disable();
}

void InspectorDOMDebuggerAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
}

void InspectorDOMDebuggerAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    disable();
}

void InspectorDOMDebuggerAgent::discardAgent()
{
    m_debuggerAgent->removeListener(*this);
    m_debuggerAgent = nullptr;
}

void InspectorDOMDebuggerAgent::mainFrameNavigated()
{
    for (auto& breakpoint : m_listenerBreakpoints.values())
        breakpoint->resetHitCount();

    if (m_pauseOnAllIntervalsBreakpoint)
        m_pauseOnAllIntervalsBreakpoint->resetHitCount();

    if (m_pauseOnAllListenersBreakpoint)
        m_pauseOnAllListenersBreakpoint->resetHitCount();

    if (m_pauseOnAllTimeoutsBreakpoint)
        m_pauseOnAllTimeoutsBreakpoint->resetHitCount();
}

Protocol::ErrorStringOr<void> InspectorDOMDebuggerAgent::setEventBreakpoint(Protocol::DOMDebugger::EventBreakpointType breakpointType, const String& eventName, RefPtr<JSON::Object>&& options)
{
    Protocol::ErrorString errorString;

    auto breakpoint = InspectorDebuggerAgent::debuggerBreakpointFromPayload(errorString, WTFMove(options));
    if (!breakpoint)
        return makeUnexpected(errorString);

    if (!eventName.isEmpty()) {
        if (breakpointType == Protocol::DOMDebugger::EventBreakpointType::Listener) {
            if (!m_listenerBreakpoints.add(eventName, breakpoint.releaseNonNull()))
                return makeUnexpected("Breakpoint with eventName already exists"_s);
            return { };
        }

        return makeUnexpected("Unexpected eventName"_s);
    }

    switch (breakpointType) {
    case Protocol::DOMDebugger::EventBreakpointType::AnimationFrame:
        if (!setAnimationFrameBreakpoint(errorString, WTFMove(breakpoint)))
            return makeUnexpected(errorString);
        return { };

    case Protocol::DOMDebugger::EventBreakpointType::Interval:
        if (m_pauseOnAllIntervalsBreakpoint)
            return makeUnexpected("Breakpoint for Interval already exists"_s);
        m_pauseOnAllIntervalsBreakpoint = WTFMove(breakpoint);
        return { };

    case Protocol::DOMDebugger::EventBreakpointType::Listener:
        if (m_pauseOnAllListenersBreakpoint)
            return makeUnexpected("Breakpoint for Listener already exists"_s);
        m_pauseOnAllListenersBreakpoint = WTFMove(breakpoint);
        return { };

    case Protocol::DOMDebugger::EventBreakpointType::Timeout:
        if (m_pauseOnAllTimeoutsBreakpoint)
            return makeUnexpected("Breakpoint for Timeout already exists"_s);
        m_pauseOnAllTimeoutsBreakpoint = WTFMove(breakpoint);
        return { };
    }

    ASSERT_NOT_REACHED();
    return makeUnexpected("Not supported");
}

Protocol::ErrorStringOr<void> InspectorDOMDebuggerAgent::removeEventBreakpoint(Protocol::DOMDebugger::EventBreakpointType breakpointType, const String& eventName)
{
    Protocol::ErrorString errorString;

    if (!eventName.isEmpty()) {
        if (breakpointType == Protocol::DOMDebugger::EventBreakpointType::Listener) {
            if (!m_listenerBreakpoints.remove(eventName))
                return makeUnexpected("Breakpoint for given eventName missing"_s);
            return { };
        }

        return makeUnexpected("Unexpected eventName"_s);
    }

    switch (breakpointType) {
    case Protocol::DOMDebugger::EventBreakpointType::AnimationFrame:
        if (!setAnimationFrameBreakpoint(errorString, nullptr))
            return makeUnexpected(errorString);
        return { };

    case Protocol::DOMDebugger::EventBreakpointType::Interval:
        if (!m_pauseOnAllIntervalsBreakpoint)
            return makeUnexpected("Breakpoint for Intervals missing"_s);
        m_pauseOnAllIntervalsBreakpoint = nullptr;
        return { };

    case Protocol::DOMDebugger::EventBreakpointType::Listener:
        if (!m_pauseOnAllListenersBreakpoint)
            return makeUnexpected("Breakpoint for Listeners missing"_s);
        m_pauseOnAllListenersBreakpoint = nullptr;
        return { };

    case Protocol::DOMDebugger::EventBreakpointType::Timeout:
        if (!m_pauseOnAllTimeoutsBreakpoint)
            return makeUnexpected("Breakpoint for Timeouts missing"_s);
        m_pauseOnAllTimeoutsBreakpoint = nullptr;
        return { };
    }

    ASSERT_NOT_REACHED();
    return makeUnexpected("Not supported");
}

void InspectorDOMDebuggerAgent::willHandleEvent(Event& event, const RegisteredEventListener& registeredEventListener)
{
    auto state = event.target()->scriptExecutionContext()->globalObject();
    auto injectedScript = m_injectedScriptManager.injectedScriptFor(state);
    if (injectedScript.hasNoValue())
        return;

    // Set Web Inspector console command line `$event` getter.
    {
        JSC::JSLockHolder lock(state);

        injectedScript.setEventValue(toJS(state, deprecatedGlobalObjectForPrototype(state), event));
    }

    if (!m_debuggerAgent->breakpointsActive())
        return;

    auto* domAgent = m_instrumentingAgents.persistentDOMAgent();

    auto breakpoint = m_pauseOnAllListenersBreakpoint;
    if (!breakpoint) {
        auto it = m_listenerBreakpoints.find(event.type());
        if (it != m_listenerBreakpoints.end())
            breakpoint = it->value.copyRef();
    }
    if (!breakpoint && domAgent)
        breakpoint = domAgent->breakpointForEventListener(*event.currentTarget(), event.type(), registeredEventListener.callback(), registeredEventListener.useCapture());
    if (!breakpoint)
        return;

    Ref<JSON::Object> eventData = JSON::Object::create();
    eventData->setString("eventName"_s, event.type());
    if (domAgent) {
        int eventListenerId = domAgent->idForEventListener(*event.currentTarget(), event.type(), registeredEventListener.callback(), registeredEventListener.useCapture());
        if (eventListenerId)
            eventData->setInteger("eventListenerId"_s, eventListenerId);
    }

    m_debuggerAgent->schedulePauseForSpecialBreakpoint(*breakpoint, Inspector::DebuggerFrontendDispatcher::Reason::Listener, WTFMove(eventData));
}

void InspectorDOMDebuggerAgent::didHandleEvent(Event& event, const RegisteredEventListener& registeredEventListener)
{
    auto state = event.target()->scriptExecutionContext()->globalObject();
    auto injectedScript = m_injectedScriptManager.injectedScriptFor(state);
    if (injectedScript.hasNoValue())
        return;

    // Clear Web Inspector console command line `$event` getter.
    {
        JSC::JSLockHolder lock(state);

        injectedScript.clearEventValue();
    }

    if (!m_debuggerAgent->breakpointsActive())
        return;

    auto breakpoint = m_pauseOnAllListenersBreakpoint;
    if (!breakpoint)
        breakpoint = m_listenerBreakpoints.get(event.type());
    if (!breakpoint) {
        if (auto* domAgent = m_instrumentingAgents.persistentDOMAgent())
            breakpoint = domAgent->breakpointForEventListener(*event.currentTarget(), event.type(), registeredEventListener.callback(), registeredEventListener.useCapture());
    }
    if (!breakpoint)
        return;

    m_debuggerAgent->cancelPauseForSpecialBreakpoint(*breakpoint);
}

void InspectorDOMDebuggerAgent::willFireTimer(bool oneShot)
{
    if (!m_debuggerAgent->breakpointsActive())
        return;

    auto breakpoint = oneShot ? m_pauseOnAllTimeoutsBreakpoint : m_pauseOnAllIntervalsBreakpoint;
    if (!breakpoint)
        return;

    auto breakReason = oneShot ? Inspector::DebuggerFrontendDispatcher::Reason::Timeout : Inspector::DebuggerFrontendDispatcher::Reason::Interval;
    m_debuggerAgent->schedulePauseForSpecialBreakpoint(*breakpoint, breakReason);
}

void InspectorDOMDebuggerAgent::didFireTimer(bool oneShot)
{
    if (!m_debuggerAgent->breakpointsActive())
        return;

    auto breakpoint = oneShot ? m_pauseOnAllTimeoutsBreakpoint : m_pauseOnAllIntervalsBreakpoint;
    if (!breakpoint)
        return;

    m_debuggerAgent->cancelPauseForSpecialBreakpoint(*breakpoint);
}

Protocol::ErrorStringOr<void> InspectorDOMDebuggerAgent::setURLBreakpoint(const String& url, std::optional<bool>&& isRegex, RefPtr<JSON::Object>&& options)
{
    Protocol::ErrorString errorString;

    auto breakpoint = InspectorDebuggerAgent::debuggerBreakpointFromPayload(errorString, WTFMove(options));
    if (!breakpoint)
        return makeUnexpected(errorString);

    if (url.isEmpty()) {
        if (m_pauseOnAllURLsBreakpoint)
            return makeUnexpected("Breakpoint for all URLs already exists"_s);
        m_pauseOnAllURLsBreakpoint = WTFMove(breakpoint);
        return { };
    }

    if (isRegex && *isRegex) {
        if (!m_urlRegexBreakpoints.add(url, breakpoint.releaseNonNull()))
            return makeUnexpected("Breakpoint for given regex already exists"_s);
    } else {
        if (!m_urlTextBreakpoints.add(url, breakpoint.releaseNonNull()))
            return makeUnexpected("Breakpoint for given URL already exists"_s);
    }

    return { };
}

Protocol::ErrorStringOr<void> InspectorDOMDebuggerAgent::removeURLBreakpoint(const String& url, std::optional<bool>&& isRegex)
{
    if (url.isEmpty()) {
        if (!m_pauseOnAllURLsBreakpoint)
            return makeUnexpected("Breakpoint for all URLs missing"_s);
        m_pauseOnAllURLsBreakpoint = nullptr;
        return { };
    }

    if (isRegex && *isRegex) {
        if (!m_urlRegexBreakpoints.remove(url))
            return makeUnexpected("Missing breakpoint for given regex"_s);
    } else {
        if (!m_urlTextBreakpoints.remove(url))
            return makeUnexpected("Missing breakpoint for given URL"_s);
    }

    return { };
}

void InspectorDOMDebuggerAgent::breakOnURLIfNeeded(const String& url, URLBreakpointSource source)
{
    if (!m_debuggerAgent->breakpointsActive())
        return;

    constexpr bool caseSensitive = false;

    auto breakpointURL = emptyString();
    auto breakpoint = m_pauseOnAllURLsBreakpoint.copyRef();
    if (!breakpoint) {
        for (auto& [query, textBreakpoint] : m_urlTextBreakpoints) {
            auto regex = ContentSearchUtilities::createRegularExpressionForSearchString(query, caseSensitive, ContentSearchUtilities::SearchStringType::ContainsString);
            if (regex.match(url) != -1) {
                breakpoint = textBreakpoint.copyRef();
                breakpointURL = query;
                break;
            }
        }
    }
    if (!breakpoint) {
        for (auto& [query, regexBreakpoint] : m_urlRegexBreakpoints) {
            auto regex = ContentSearchUtilities::createRegularExpressionForSearchString(query, caseSensitive, ContentSearchUtilities::SearchStringType::Regex);
            if (regex.match(url) != -1) {
                breakpoint = regexBreakpoint.copyRef();
                breakpointURL = query;
                break;
            }
        }
    }
    if (!breakpoint)
        return;

    auto breakReason = Inspector::DebuggerFrontendDispatcher::Reason::Other;
    switch (source) {
    case URLBreakpointSource::Fetch:
        breakReason = Inspector::DebuggerFrontendDispatcher::Reason::Fetch;
        break;

    case URLBreakpointSource::XHR:
        breakReason = Inspector::DebuggerFrontendDispatcher::Reason::XHR;
        break;
    }

    Ref<JSON::Object> eventData = JSON::Object::create();
    eventData->setString("breakpointURL", breakpointURL);
    eventData->setString("url", url);
    m_debuggerAgent->breakProgram(breakReason, WTFMove(eventData), WTFMove(breakpoint));
}

void InspectorDOMDebuggerAgent::willSendXMLHttpRequest(const String& url)
{
    breakOnURLIfNeeded(url, URLBreakpointSource::XHR);
}

void InspectorDOMDebuggerAgent::willFetch(const String& url)
{
    breakOnURLIfNeeded(url, URLBreakpointSource::Fetch);
}

} // namespace WebCore
