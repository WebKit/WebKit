/*
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

#ifndef InspectorConsoleInstrumentation_h
#define InspectorConsoleInstrumentation_h

#include "InspectorInstrumentation.h"
#include "ScriptProfile.h"
#include <inspector/ScriptArguments.h>
#include <inspector/ScriptCallStack.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

inline void InspectorInstrumentation::addMessageToConsole(Page* page, MessageSource source, MessageType type, MessageLevel level, const String& message, PassRefPtr<Inspector::ScriptCallStack> callStack, unsigned long requestIdentifier)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        addMessageToConsoleImpl(instrumentingAgents, source, type, level, message, callStack, requestIdentifier);
#else
    UNUSED_PARAM(page);
    UNUSED_PARAM(source);
    UNUSED_PARAM(type);
    UNUSED_PARAM(level);
    UNUSED_PARAM(message);
    UNUSED_PARAM(callStack);
    UNUSED_PARAM(requestIdentifier);
#endif
}

inline void InspectorInstrumentation::addMessageToConsole(Page* page, MessageSource source, MessageType type, MessageLevel level, const String& message, JSC::ExecState* state, PassRefPtr<Inspector::ScriptArguments> arguments, unsigned long requestIdentifier)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        addMessageToConsoleImpl(instrumentingAgents, source, type, level, message, state, arguments, requestIdentifier);
#else
    UNUSED_PARAM(page);
    UNUSED_PARAM(source);
    UNUSED_PARAM(type);
    UNUSED_PARAM(level);
    UNUSED_PARAM(message);
    UNUSED_PARAM(state);
    UNUSED_PARAM(arguments);
    UNUSED_PARAM(requestIdentifier);
#endif
}

inline void InspectorInstrumentation::addMessageToConsole(Page* page, MessageSource source, MessageType type, MessageLevel level, const String& message, const String& scriptID, unsigned lineNumber, unsigned columnNumber, JSC::ExecState* state, unsigned long requestIdentifier)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        addMessageToConsoleImpl(instrumentingAgents, source, type, level, message, scriptID, lineNumber, columnNumber, state, requestIdentifier);
#else
    UNUSED_PARAM(page);
    UNUSED_PARAM(source);
    UNUSED_PARAM(type);
    UNUSED_PARAM(level);
    UNUSED_PARAM(message);
    UNUSED_PARAM(scriptID);
    UNUSED_PARAM(lineNumber);
    UNUSED_PARAM(state);
    UNUSED_PARAM(requestIdentifier);
    UNUSED_PARAM(columnNumber);
#endif
}

inline void InspectorInstrumentation::addMessageToConsole(WorkerGlobalScope* workerGlobalScope, MessageSource source, MessageType type, MessageLevel level, const String& message, PassRefPtr<Inspector::ScriptCallStack> callStack, unsigned long requestIdentifier)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForWorkerGlobalScope(workerGlobalScope))
        addMessageToConsoleImpl(instrumentingAgents, source, type, level, message, callStack, requestIdentifier);
#else
    UNUSED_PARAM(workerGlobalScope);
    UNUSED_PARAM(source);
    UNUSED_PARAM(type);
    UNUSED_PARAM(level);
    UNUSED_PARAM(message);
    UNUSED_PARAM(callStack);
    UNUSED_PARAM(requestIdentifier);
#endif
}

inline void InspectorInstrumentation::addMessageToConsole(WorkerGlobalScope* workerGlobalScope, MessageSource source, MessageType type, MessageLevel level, const String& message, const String& scriptID, unsigned lineNumber, unsigned columnNumber, JSC::ExecState* state, unsigned long requestIdentifier)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForWorkerGlobalScope(workerGlobalScope))
        addMessageToConsoleImpl(instrumentingAgents, source, type, level, message, scriptID, lineNumber, columnNumber, state, requestIdentifier);
#else
    UNUSED_PARAM(workerGlobalScope);
    UNUSED_PARAM(source);
    UNUSED_PARAM(type);
    UNUSED_PARAM(level);
    UNUSED_PARAM(message);
    UNUSED_PARAM(scriptID);
    UNUSED_PARAM(lineNumber);
    UNUSED_PARAM(columnNumber);
    UNUSED_PARAM(state);
    UNUSED_PARAM(requestIdentifier);
#endif
}

inline void InspectorInstrumentation::consoleCount(Page* page, JSC::ExecState* state, PassRefPtr<Inspector::ScriptArguments> arguments)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        consoleCountImpl(instrumentingAgents, state, arguments);
#else
    UNUSED_PARAM(page);
    UNUSED_PARAM(state);
    UNUSED_PARAM(arguments);
#endif
}

inline void InspectorInstrumentation::startConsoleTiming(Frame* frame, const String& title)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        startConsoleTimingImpl(instrumentingAgents, frame, title);
#else
    UNUSED_PARAM(frame);
    UNUSED_PARAM(title);
#endif
}

inline void InspectorInstrumentation::stopConsoleTiming(Frame* frame, const String& title, PassRefPtr<Inspector::ScriptCallStack> stack)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        stopConsoleTimingImpl(instrumentingAgents, frame, title, stack);
#else
    UNUSED_PARAM(frame);
    UNUSED_PARAM(title);
    UNUSED_PARAM(stack);
#endif
}

inline void InspectorInstrumentation::consoleTimeStamp(Frame* frame, PassRefPtr<Inspector::ScriptArguments> arguments)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        consoleTimeStampImpl(instrumentingAgents, frame, arguments);
#else
    UNUSED_PARAM(frame);
    UNUSED_PARAM(arguments);
#endif
}

inline void InspectorInstrumentation::startProfiling(Page* page, JSC::ExecState* exec, const String &title)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        startProfilingImpl(instrumentingAgents, exec, title);
#else
    UNUSED_PARAM(page);
    UNUSED_PARAM(title);
#endif
}

inline PassRefPtr<JSC::Profile> InspectorInstrumentation::stopProfiling(Page* page, JSC::ExecState* exec, const String &title)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return stopProfilingImpl(instrumentingAgents, exec, title);
#else
    UNUSED_PARAM(page);
    UNUSED_PARAM(title);
#endif
    return nullptr;
}

} // namespace WebCore

#endif // !defined(InspectorConsoleInstrumentation_h)
