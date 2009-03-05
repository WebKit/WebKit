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

#include "JSInspectedObjectWrapper.h"
#include "ScriptString.h"
#include "ScriptCallStack.h"
#include "ScriptCallFrame.h"

#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSValueRef.h>
#include <runtime/JSLock.h>

using namespace JSC;

namespace WebCore {

ConsoleMessage::ConsoleMessage(MessageSource s, MessageLevel l, const String& m, unsigned li, const String& u, unsigned g)
    : source(s)
    , level(l)
    , message(m)
    , line(li)
    , url(u)
    , groupLevel(g)
    , repeatCount(1)
{
}

ConsoleMessage::ConsoleMessage(MessageSource s, MessageLevel l, ScriptCallStack* callStack, unsigned g, bool storeTrace)
    : source(s)
    , level(l)
    , wrappedArguments(callStack->at(0).argumentCount())
    , frames(storeTrace ? callStack->size() : 0)
    , groupLevel(g)
    , repeatCount(1)
{
    const ScriptCallFrame& lastCaller = callStack->at(0);
    line = lastCaller.lineNumber();
    url = lastCaller.sourceURL().string();

    // FIXME: For now, just store function names as strings.
    // As ScriptCallStack start storing line number and source URL for all
    // frames, refactor to use that, as well.
    if (storeTrace) {
        unsigned stackSize = callStack->size();
        for (unsigned i = 0; i < stackSize; ++i)
            frames[i] = callStack->at(i).functionName();
    }

    JSLock lock(false);

    for (unsigned i = 0; i < lastCaller.argumentCount(); ++i)
        wrappedArguments[i] = JSInspectedObjectWrapper::wrap(callStack->state(), lastCaller.argumentAt(i).jsValue());
}

bool ConsoleMessage::isEqual(ExecState* exec, ConsoleMessage* msg) const
{
    if (msg->wrappedArguments.size() != this->wrappedArguments.size() ||
        (!exec && msg->wrappedArguments.size()))
        return false;

    for (size_t i = 0; i < msg->wrappedArguments.size(); ++i) {
        ASSERT_ARG(exec, exec);
        if (!JSValueIsEqual(toRef(exec), toRef(msg->wrappedArguments[i].get()), toRef(this->wrappedArguments[i].get()), 0))
            return false;
    }

    size_t frameCount = msg->frames.size();
    if (frameCount != this->frames.size())
        return false;

    for (size_t i = 0; i < frameCount; ++i) {
        const ScriptString& myFrameFunctionName = this->frames[i];
        if (myFrameFunctionName != msg->frames[i])
            return false;
    }

    return msg->source == this->source
    && msg->level == this->level
    && msg->message == this->message
    && msg->line == this->line
    && msg->url == this->url
    && msg->groupLevel == this->groupLevel;
}

} // namespace WebCore
