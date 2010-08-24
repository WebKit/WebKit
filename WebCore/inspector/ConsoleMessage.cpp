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
#include "InspectorValues.h"
#include "ScriptCallStack.h"
#include "ScriptValue.h"

#if ENABLE(INSPECTOR)
#include "InspectorFrontend.h"
#endif

namespace WebCore {

ConsoleMessage::CallFrame::CallFrame(const ScriptCallFrame& frame)
    : m_functionName(frame.functionName())
    , m_sourceURL(frame.sourceURL())
    , m_lineNumber(frame.lineNumber())
{
}

ConsoleMessage::CallFrame::CallFrame()
    : m_lineNumber(0)
{
}

bool ConsoleMessage::CallFrame::isEqual(const ConsoleMessage::CallFrame& o) const
{
    return m_functionName == o.m_functionName
        && m_sourceURL == o.m_sourceURL
        && m_lineNumber == o.m_lineNumber;
}

#if ENABLE(INSPECTOR)
PassRefPtr<InspectorObject> ConsoleMessage::CallFrame::buildInspectorObject() const
{
    RefPtr<InspectorObject> frame = InspectorObject::create();
    frame->setString("functionName", m_functionName);
    frame->setString("sourceURL", m_sourceURL.string());
    frame->setNumber("lineNumber", m_lineNumber);
    return frame;
}
#endif

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

ConsoleMessage::ConsoleMessage(MessageSource s, MessageType t, MessageLevel l, const String& m, ScriptCallStack* callStack, unsigned g, bool storeTrace)
    : m_source(s)
    , m_type(t)
    , m_level(l)
    , m_message(m)
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

    if (storeTrace) {
        for (unsigned i = 0; i < callStack->size(); ++i)
            m_frames[i] = ConsoleMessage::CallFrame(callStack->at(i));
    }

#if ENABLE(INSPECTOR)
    for (unsigned i = 0; i < lastCaller.argumentCount(); ++i)
        m_arguments[i] = lastCaller.argumentAt(i);
#endif
}

#if ENABLE(INSPECTOR)
void ConsoleMessage::addToFrontend(InspectorFrontend* frontend, InjectedScriptHost* injectedScriptHost)
{
    RefPtr<InspectorObject> jsonObj = InspectorObject::create();
    jsonObj->setNumber("source", static_cast<int>(m_source));
    jsonObj->setNumber("type", static_cast<int>(m_type));
    jsonObj->setNumber("level", static_cast<int>(m_level));
    jsonObj->setNumber("line", static_cast<int>(m_line));
    jsonObj->setString("url", m_url);
    jsonObj->setNumber("groupLevel", static_cast<int>(m_groupLevel));
    jsonObj->setNumber("repeatCount", static_cast<int>(m_repeatCount));
    jsonObj->setString("message", m_message);
    if (!m_arguments.isEmpty()) {
        InjectedScript injectedScript = injectedScriptHost->injectedScriptFor(m_scriptState.get());
        if (!injectedScript.hasNoValue()) {
            RefPtr<InspectorArray> jsonArgs = InspectorArray::create();
            for (unsigned i = 0; i < m_arguments.size(); ++i) {
                RefPtr<InspectorValue> inspectorValue = injectedScript.wrapForConsole(m_arguments[i]);
                if (!inspectorValue) {
                    ASSERT_NOT_REACHED();
                    return;
                }
                jsonArgs->pushValue(inspectorValue);
            }
            jsonObj->setArray("parameters", jsonArgs);
        }
    }
    if (!m_frames.isEmpty()) {
        RefPtr<InspectorArray> frames = InspectorArray::create();
        for (unsigned i = 0; i < m_frames.size(); i++)
            frames->pushObject(m_frames.at(i).buildInspectorObject());
        jsonObj->setArray("stackTrace", frames);
    }
    frontend->addConsoleMessage(jsonObj);
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
        if (!m_frames[i].isEqual(msg->m_frames[i]))
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
