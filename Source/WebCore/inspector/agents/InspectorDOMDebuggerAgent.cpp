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

void InspectorDOMDebuggerAgent::setEventBreakpoint(ErrorString& errorString, const String& breakpointTypeString, const String* eventName, const JSON::Object* optionsPayload)
{
    if (breakpointTypeString.isEmpty()) {
        errorString = "breakpointType is empty"_s;
        return;
    }

    auto breakpointType = Inspector::Protocol::InspectorHelpers::parseEnumValueFromString<Inspector::Protocol::DOMDebugger::EventBreakpointType>(breakpointTypeString);
    if (!breakpointType) {
        errorString = makeString("Unknown breakpointType: "_s, breakpointTypeString);
        return;
    }

    auto breakpoint = InspectorDebuggerAgent::debuggerBreakpointFromPayload(errorString, optionsPayload);
    if (!breakpoint)
        return;

    if (eventName && !eventName->isEmpty()) {
        if (breakpointType.value() == Inspector::Protocol::DOMDebugger::EventBreakpointType::Listener) {
            if (!m_listenerBreakpoints.add(*eventName, breakpoint.releaseNonNull()))
                errorString = "Breakpoint with eventName already exists"_s;
            return;
        }

        errorString = "Unexpected eventName"_s;
        return;
    }

    switch (breakpointType.value()) {
    case Inspector::Protocol::DOMDebugger::EventBreakpointType::AnimationFrame:
        setAnimationFrameBreakpoint(errorString, WTFMove(breakpoint));
        break;

    case Inspector::Protocol::DOMDebugger::EventBreakpointType::Interval:
        if (!m_pauseOnAllIntervalsBreakpoint)
            m_pauseOnAllIntervalsBreakpoint = WTFMove(breakpoint);
        else
            errorString = "Breakpoint for Interval already exists"_s;
        break;

    case Inspector::Protocol::DOMDebugger::EventBreakpointType::Listener:
        if (!m_pauseOnAllListenersBreakpoint)
            m_pauseOnAllListenersBreakpoint = WTFMove(breakpoint);
        else
            errorString = "Breakpoint for Listener already exists"_s;
        break;

    case Inspector::Protocol::DOMDebugger::EventBreakpointType::Timeout:
        if (!m_pauseOnAllTimeoutsBreakpoint)
            m_pauseOnAllTimeoutsBreakpoint = WTFMove(breakpoint);
        else
            errorString = "Breakpoint for Timeout already exists"_s;
        break;
    }
}

void InspectorDOMDebuggerAgent::removeEventBreakpoint(ErrorString& errorString, const String& breakpointTypeString, const String* eventName)
{
    if (breakpointTypeString.isEmpty()) {
        errorString = "breakpointType is empty"_s;
        return;
    }

    auto breakpointType = Inspector::Protocol::InspectorHelpers::parseEnumValueFromString<Inspector::Protocol::DOMDebugger::EventBreakpointType>(breakpointTypeString);
    if (!breakpointType) {
        errorString = makeString("Unknown breakpointType: "_s, breakpointTypeString);
        return;
    }

    if (eventName && !eventName->isEmpty()) {
        if (breakpointType.value() == Inspector::Protocol::DOMDebugger::EventBreakpointType::Listener) {
            if (!m_listenerBreakpoints.remove(*eventName))
                errorString = "Breakpoint for given eventName missing"_s;
            return;
        }

        errorString = "Unexpected eventName"_s;
        return;
    }

    switch (breakpointType.value()) {
    case Inspector::Protocol::DOMDebugger::EventBreakpointType::AnimationFrame:
        setAnimationFrameBreakpoint(errorString, nullptr);
        break;

    case Inspector::Protocol::DOMDebugger::EventBreakpointType::Interval:
        if (!m_pauseOnAllIntervalsBreakpoint)
            errorString = "Breakpoint for Intervals missing"_s;
        m_pauseOnAllIntervalsBreakpoint = nullptr;
        break;

    case Inspector::Protocol::DOMDebugger::EventBreakpointType::Listener:
        if (!m_pauseOnAllListenersBreakpoint)
            errorString = "Breakpoint for Listeners missing"_s;
        m_pauseOnAllListenersBreakpoint = nullptr;
        break;

    case Inspector::Protocol::DOMDebugger::EventBreakpointType::Timeout:
        if (!m_pauseOnAllTimeoutsBreakpoint)
            errorString = "Breakpoint for Timeouts missing"_s;
        m_pauseOnAllTimeoutsBreakpoint = nullptr;
        break;
    }
}

void InspectorDOMDebuggerAgent::willHandleEvent(Event& event, const RegisteredEventListener& registeredEventListener)
{
    auto state = event.target()->scriptExecutionContext()->execState();
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
    auto state = event.target()->scriptExecutionContext()->execState();
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

void InspectorDOMDebuggerAgent::setURLBreakpoint(ErrorString& errorString, const String& url, const bool* optionalIsRegex, const JSON::Object* optionsPayload)
{
    auto breakpoint = InspectorDebuggerAgent::debuggerBreakpointFromPayload(errorString, optionsPayload);
    if (!breakpoint)
        return;

    if (url.isEmpty()) {
        if (!m_pauseOnAllURLsBreakpoint)
            m_pauseOnAllURLsBreakpoint = WTFMove(breakpoint);
        else
            errorString = "Breakpoint for all URLs already exists"_s;
        return;
    }

    if (optionalIsRegex && *optionalIsRegex) {
        if (!m_urlRegexBreakpoints.add(url, breakpoint.releaseNonNull()))
            errorString = "Breakpoint for given regex already exists"_s;
    } else {
        if (!m_urlTextBreakpoints.add(url, breakpoint.releaseNonNull()))
            errorString = "Breakpoint for given URL already exists"_s;
    }
}

void InspectorDOMDebuggerAgent::removeURLBreakpoint(ErrorString& errorString, const String& url, const bool* optionalIsRegex)
{
    if (url.isEmpty()) {
        if (!m_pauseOnAllURLsBreakpoint)
            errorString = "Breakpoint for all URLs missing"_s;
        m_pauseOnAllURLsBreakpoint = nullptr;
        return;
    }

    if (optionalIsRegex && *optionalIsRegex) {
        if (!m_urlRegexBreakpoints.remove(url))
            errorString = "Missing breakpoint for given regex"_s;
    } else {
        if (!m_urlTextBreakpoints.remove(url))
            errorString = "Missing breakpoint for given URL"_s;
    }
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
