/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
#include "Console.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "ConsoleAPITypes.h"
#include "ConsoleTypes.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "InspectorConsoleInstrumentation.h"
#include "InspectorController.h"
#include "Page.h"
#include "PageConsole.h"
#include "PageGroup.h"
#include "ScriptArguments.h"
#include "ScriptCallStack.h"
#include "ScriptCallStackFactory.h"
#include "ScriptProfile.h"
#include "ScriptProfiler.h"
#include "ScriptableDocumentParser.h"
#include "Settings.h"
#include <bindings/ScriptValue.h>
#include <stdio.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

Console::Console(Frame* frame)
    : DOMWindowProperty(frame)
{
}

Console::~Console()
{
}

static void internalAddMessage(Page* page, MessageType type, MessageLevel level, JSC::ExecState* state, PassRefPtr<ScriptArguments> prpArguments, bool acceptNoArguments = false, bool printTrace = false)
{
    RefPtr<ScriptArguments> arguments = prpArguments;

    if (!page)
        return;

    if (!acceptNoArguments && !arguments->argumentCount())
        return;

    size_t stackSize = printTrace ? ScriptCallStack::maxCallStackSizeToCapture : 1;
    RefPtr<ScriptCallStack> callStack(createScriptCallStack(state, stackSize));
    const ScriptCallFrame& lastCaller = callStack->at(0);

    String message;
    bool gotMessage = arguments->getFirstArgumentAsString(message);
    InspectorInstrumentation::addMessageToConsole(page, ConsoleAPIMessageSource, type, level, message, state, arguments);

    if (page->settings().privateBrowsingEnabled())
        return;

    if (gotMessage)
        page->chrome().client().addMessageToConsole(ConsoleAPIMessageSource, type, level, message, lastCaller.lineNumber(), lastCaller.columnNumber(), lastCaller.sourceURL());

    if (!page->settings().logsPageMessagesToSystemConsoleEnabled() && !PageConsole::shouldPrintExceptions())
        return;

    PageConsole::printSourceURLAndPosition(lastCaller.sourceURL(), lastCaller.lineNumber());

    printf(": ");

    PageConsole::printMessageSourceAndLevelPrefix(ConsoleAPIMessageSource, level, printTrace);

    for (size_t i = 0; i < arguments->argumentCount(); ++i) {
        String argAsString = arguments->argumentAt(i).toString(arguments->globalState());
        printf(" %s", argAsString.utf8().data());
    }

    printf("\n");

    if (!printTrace)
        return;

    for (size_t i = 0; i < callStack->size(); ++i) {
        const ScriptCallFrame& callFrame = callStack->at(i);

        String functionName = String(callFrame.functionName());
        if (functionName.isEmpty())
            functionName = ASCIILiteral("(unknown)");

        printf("%lu: %s (", static_cast<unsigned long>(i), functionName.utf8().data());

        PageConsole::printSourceURLAndPosition(callFrame.sourceURL(), callFrame.lineNumber());

        printf(")\n");
    }
}

void Console::debug(JSC::ExecState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), LogMessageType, DebugMessageLevel, state, arguments);
}

void Console::error(JSC::ExecState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), LogMessageType, ErrorMessageLevel, state, arguments);
}

void Console::info(JSC::ExecState* state, PassRefPtr<ScriptArguments> arguments)
{
    log(state, arguments);
}

void Console::log(JSC::ExecState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), LogMessageType, LogMessageLevel, state, arguments);
}

void Console::warn(JSC::ExecState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), LogMessageType, WarningMessageLevel, state, arguments);
}

void Console::dir(JSC::ExecState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), DirMessageType, LogMessageLevel, state, arguments);
}

void Console::dirxml(JSC::ExecState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), DirXMLMessageType, LogMessageLevel, state, arguments);
}

void Console::table(JSC::ExecState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), TableMessageType, LogMessageLevel, state, arguments);
}

void Console::clear(JSC::ExecState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), ClearMessageType, LogMessageLevel, state, arguments, true);
}

void Console::trace(JSC::ExecState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), TraceMessageType, LogMessageLevel, state, arguments, true, true);
}

void Console::assertCondition(JSC::ExecState* state, PassRefPtr<ScriptArguments> arguments, bool condition)
{
    if (condition)
        return;

    internalAddMessage(page(), AssertMessageType, ErrorMessageLevel, state, arguments, true);
}

void Console::count(JSC::ExecState* state, PassRefPtr<ScriptArguments> arguments)
{
    InspectorInstrumentation::consoleCount(page(), state, arguments);
}

void Console::profile(JSC::ExecState* state, const String& title)
{
    Page* page = this->page();
    if (!page)
        return;

    // FIXME: log a console message when profiling is disabled.
    if (!InspectorInstrumentation::profilerEnabled(page))
        return;

    String resolvedTitle = title;
    if (title.isNull()) // no title so give it the next user initiated profile title.
        resolvedTitle = InspectorInstrumentation::getCurrentUserInitiatedProfileName(page, true);

    ScriptProfiler::start(state, resolvedTitle);

    RefPtr<ScriptCallStack> callStack(createScriptCallStack(state, 1));
    const ScriptCallFrame& lastCaller = callStack->at(0);
    InspectorInstrumentation::addStartProfilingMessageToConsole(page, resolvedTitle, lastCaller.lineNumber(), lastCaller.columnNumber(), lastCaller.sourceURL());
}

void Console::profileEnd(JSC::ExecState* state, const String& title)
{
    Page* page = this->page();
    if (!page)
        return;

    if (!InspectorInstrumentation::profilerEnabled(page))
        return;

    RefPtr<ScriptProfile> profile = ScriptProfiler::stop(state, title);
    if (!profile)
        return;

    m_profiles.append(profile);
    RefPtr<ScriptCallStack> callStack(createScriptCallStack(state, 1));
    InspectorInstrumentation::addProfile(page, profile, callStack);
}

void Console::time(const String& title)
{
    InspectorInstrumentation::startConsoleTiming(m_frame, title);
}

void Console::timeEnd(JSC::ExecState* state, const String& title)
{
    RefPtr<ScriptCallStack> callStack(createScriptCallStackForConsole(state));
    InspectorInstrumentation::stopConsoleTiming(m_frame, title, callStack.release());
}

void Console::timeStamp(PassRefPtr<ScriptArguments> arguments)
{
    InspectorInstrumentation::consoleTimeStamp(m_frame, arguments);
}

void Console::group(JSC::ExecState* state, PassRefPtr<ScriptArguments> arguments)
{
    InspectorInstrumentation::addMessageToConsole(page(), ConsoleAPIMessageSource, StartGroupMessageType, LogMessageLevel, String(), state, arguments);
}

void Console::groupCollapsed(JSC::ExecState* state, PassRefPtr<ScriptArguments> arguments)
{
    InspectorInstrumentation::addMessageToConsole(page(), ConsoleAPIMessageSource, StartGroupCollapsedMessageType, LogMessageLevel, String(), state, arguments);
}

void Console::groupEnd()
{
    InspectorInstrumentation::addMessageToConsole(page(), ConsoleAPIMessageSource, EndGroupMessageType, LogMessageLevel, String(), String(), 0, 0);
}

Page* Console::page() const
{
    if (!m_frame)
        return 0;
    return m_frame->page();
}

} // namespace WebCore
