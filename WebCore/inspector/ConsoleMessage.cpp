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

#include "ScriptCallStack.h"
#include "ScriptCallFrame.h"
#include "ScriptFunctionCall.h"
#include "ScriptObjectQuarantine.h"
#include "ScriptString.h"

namespace WebCore {

ConsoleMessage::ConsoleMessage(MessageSource s, MessageLevel l, const String& m, unsigned li, const String& u, unsigned g)
    : m_source(s)
    , m_level(l)
    , m_message(m)
    , m_line(li)
    , m_url(u)
    , m_groupLevel(g)
    , m_repeatCount(1)
{
}

ConsoleMessage::ConsoleMessage(MessageSource s, MessageLevel l, ScriptCallStack* callStack, unsigned g, bool storeTrace)
    : m_source(s)
    , m_level(l)
    , m_wrappedArguments(callStack->at(0).argumentCount())
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

    for (unsigned i = 0; i < lastCaller.argumentCount(); ++i)
        m_wrappedArguments[i] = quarantineValue(callStack->state(), lastCaller.argumentAt(i));
}

void ConsoleMessage::addToConsole(ScriptState* scriptState, const ScriptObject& webInspector)
{
    ScriptFunctionCall messageConstructor(scriptState, webInspector, "ConsoleMessage");
    messageConstructor.appendArgument(static_cast<unsigned int>(m_source));
    messageConstructor.appendArgument(static_cast<unsigned int>(m_level));
    messageConstructor.appendArgument(m_line);
    messageConstructor.appendArgument(m_url);
    messageConstructor.appendArgument(m_groupLevel);
    messageConstructor.appendArgument(m_repeatCount);

    if (!m_frames.isEmpty()) {
        for (unsigned i = 0; i < m_frames.size(); ++i)
            messageConstructor.appendArgument(m_frames[i]);
    } else if (!m_wrappedArguments.isEmpty()) {
        for (unsigned i = 0; i < m_wrappedArguments.size(); ++i)
            messageConstructor.appendArgument(m_wrappedArguments[i]);
    } else
        messageConstructor.appendArgument(m_message);

    bool hadException = false;
    ScriptObject message = messageConstructor.construct(hadException, false);
    if (hadException)
        return;

    ScriptFunctionCall addMessageToConsole(scriptState, webInspector, "addMessageToConsole");
    addMessageToConsole.appendArgument(message);
    addMessageToConsole.call(hadException, false);
}

bool ConsoleMessage::isEqual(ScriptState* state, ConsoleMessage* msg) const
{
    if (msg->m_wrappedArguments.size() != m_wrappedArguments.size())
        return false;
    if (!state && msg->m_wrappedArguments.size())
        return false;

    ASSERT_ARG(state, state || msg->m_wrappedArguments.isEmpty());

    for (size_t i = 0; i < msg->m_wrappedArguments.size(); ++i) {
        if (!m_wrappedArguments[i].isEqual(state, msg->m_wrappedArguments[i]))
            return false;
    }

    size_t frameCount = msg->m_frames.size();
    if (frameCount != m_frames.size())
        return false;

    for (size_t i = 0; i < frameCount; ++i) {
        if (m_frames[i] != msg->m_frames[i])
            return false;
    }

    return msg->m_source == m_source
        && msg->m_level == m_level
        && msg->m_message == m_message
        && msg->m_line == m_line
        && msg->m_url == m_url
        && msg->m_groupLevel == m_groupLevel;
}

} // namespace WebCore
