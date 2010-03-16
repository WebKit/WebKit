/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#include "ConsoleMessage.h"

#include "InjectedScript.h"
#include "InjectedScriptHost.h"
#include "InspectorFrontend.h"
#include "ScriptCallStack.h"
#include "ScriptObject.h"
#include "SerializedScriptValue.h"

namespace WebCore {

ConsoleMessage::ConsoleMessage(MessageSource s, MessageType t, MessageLevel l, const String& m, unsigned li, const String& u, unsigned g)
    : m_source(s)
    , m_type(t)
    , m_level(l)
    , m_message(m)
    , m_line(li)
    , m_url(u)
    , m_groupLevel(g)
    , m_repeatCount(1)
{
}

ConsoleMessage::ConsoleMessage(MessageSource s, MessageType t, MessageLevel l, ScriptCallStack* callStack, unsigned g, bool storeTrace)
    : m_source(s)
    , m_type(t)
    , m_level(l)
#if ENABLE(INSPECTOR)
    , m_arguments(callStack->at(0).argumentCount())
    , m_scriptState(callStack->globalState())
#endif
    , m_frames(storeTrace ? callStack->size() : 0)
    , m_groupLevel(g)
    , m_repeatCount(1)
{
    const ScriptCallFrame& lastCaller = callStack->at(0);
    m_line = lastCaller.lineNumber();
    m_url = lastCaller.sourceURL().string();

    // FIXME: For now, just store function names as strings.
    // As ScriptCallStack start storing line number and source URL for all
    // frames, refactor to use that, as well.
    if (storeTrace) {
        for (unsigned i = 0; i < callStack->size(); ++i)
            m_frames[i] = callStack->at(i).functionName();
    }

#if ENABLE(INSPECTOR)
    for (unsigned i = 0; i < lastCaller.argumentCount(); ++i)
        m_arguments[i] = lastCaller.argumentAt(i);
#endif
}

#if ENABLE(INSPECTOR)
void ConsoleMessage::addToFrontend(InspectorFrontend* frontend, InjectedScriptHost* injectedScriptHost)
{
    ScriptObject jsonObj = frontend->newScriptObject();
    jsonObj.set("source", static_cast<int>(m_source));
    jsonObj.set("type", static_cast<int>(m_type));
    jsonObj.set("level", static_cast<int>(m_level));
    jsonObj.set("line", static_cast<int>(m_line));
    jsonObj.set("url", m_url);
    jsonObj.set("groupLevel", static_cast<int>(m_groupLevel));
    jsonObj.set("repeatCount", static_cast<int>(m_repeatCount));
    Vector<RefPtr<SerializedScriptValue> > arguments;
    if (!m_arguments.isEmpty()) {
        InjectedScript injectedScript = injectedScriptHost->injectedScriptFor(m_scriptState);
        for (unsigned i = 0; i < m_arguments.size(); ++i) {
            RefPtr<SerializedScriptValue> serializedValue = injectedScript.wrapForConsole(m_arguments[i]);
            arguments.append(serializedValue);
        }
    }   
    frontend->addConsoleMessage(jsonObj, m_frames, arguments,  m_message);
}

void ConsoleMessage::updateRepeatCountInConsole(InspectorFrontend* frontend)
{
    frontend->updateConsoleMessageRepeatCount(m_repeatCount);
}
#endif // ENABLE(INSPECTOR)

bool ConsoleMessage::isEqual(ScriptState* state, ConsoleMessage* msg) const
{
#if ENABLE(INSPECTOR)
    if (msg->m_arguments.size() != m_arguments.size())
        return false;
    if (!state && msg->m_arguments.size())
        return false;

    ASSERT_ARG(state, state || msg->m_arguments.isEmpty());

    for (size_t i = 0; i < msg->m_arguments.size(); ++i) {
        if (!m_arguments[i].isEqual(state, msg->m_arguments[i]))
            return false;
    }
#else
    UNUSED_PARAM(state);
#endif // ENABLE(INSPECTOR)

    size_t frameCount = msg->m_frames.size();
    if (frameCount != m_frames.size())
        return false;

    for (size_t i = 0; i < frameCount; ++i) {
        if (m_frames[i] != msg->m_frames[i])
            return false;
    }

    return msg->m_source == m_source
        && msg->m_type == m_type
        && msg->m_level == m_level
        && msg->m_message == m_message
        && msg->m_line == m_line
        && msg->m_url == m_url
        && msg->m_groupLevel == m_groupLevel;
}

} // namespace WebCore
