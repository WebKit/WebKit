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
#include "MemoryInfo.h"
#include "Page.h"
#include "PageGroup.h"
#include "ScriptArguments.h"
#include "ScriptCallStack.h"
#include "ScriptCallStackFactory.h"
#include "ScriptProfile.h"
#include "ScriptProfiler.h"
#include "ScriptValue.h"
#include "ScriptableDocumentParser.h"
#include "Settings.h"
#include <stdio.h>
#include <wtf/UnusedParam.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(CHROMIUM)
#include "TraceEvent.h"
#endif

namespace WebCore {

namespace {
    int muteCount = 0;
}

Console::Console(Frame* frame)
    : DOMWindowProperty(frame)
{
}

Console::~Console()
{
}

static void printSourceURLAndLine(const String& sourceURL, unsigned lineNumber)
{
    if (!sourceURL.isEmpty()) {
        if (lineNumber > 0)
            printf("%s:%d: ", sourceURL.utf8().data(), lineNumber);
        else
            printf("%s: ", sourceURL.utf8().data());
    }
}

static void printMessageSourceAndLevelPrefix(MessageSource source, MessageLevel level)
{
    const char* sourceString;
    switch (source) {
    case HTMLMessageSource:
        sourceString = "HTML";
        break;
    case XMLMessageSource:
        sourceString = "XML";
        break;
    case JSMessageSource:
        sourceString = "JS";
        break;
    case NetworkMessageSource:
        sourceString = "NETWORK";
        break;
    case ConsoleAPIMessageSource:
        sourceString = "CONSOLEAPI";
        break;
    case OtherMessageSource:
        sourceString = "OTHER";
        break;
    default:
        ASSERT_NOT_REACHED();
        sourceString = "UNKNOWN";
        break;
    }

    const char* levelString;
    switch (level) {
    case TipMessageLevel:
        levelString = "TIP";
        break;
    case LogMessageLevel:
        levelString = "LOG";
        break;
    case WarningMessageLevel:
        levelString = "WARN";
        break;
    case ErrorMessageLevel:
        levelString = "ERROR";
        break;
    case DebugMessageLevel:
        levelString = "DEBUG";
        break;
    default:
        ASSERT_NOT_REACHED();
        levelString = "UNKNOWN";
        break;
    }

    printf("%s %s:", sourceString, levelString);
}

void Console::addMessage(MessageSource source, MessageLevel level, const String& message, unsigned long requestIdentifier, Document* document)
{
    String url;
    if (document)
        url = document->url().string();
    unsigned line = 0;
    if (document && document->parsing() && !document->isInDocumentWrite() && document->scriptableDocumentParser()) {
        ScriptableDocumentParser* parser = document->scriptableDocumentParser();
        if (!parser->isWaitingForScripts() && !parser->isExecutingScript())
            line = parser->lineNumber().oneBasedInt();
    }
    addMessage(source, level, message, url, line, 0, 0, requestIdentifier);
}

void Console::addMessage(MessageSource source, MessageLevel level, const String& message, PassRefPtr<ScriptCallStack> callStack)
{
    addMessage(source, level, message, String(), 0, callStack, 0);
}

void Console::addMessage(MessageSource source, MessageLevel level, const String& message, const String& url, unsigned lineNumber, PassRefPtr<ScriptCallStack> callStack, ScriptState* state, unsigned long requestIdentifier)
{
    if (muteCount && source != ConsoleAPIMessageSource)
        return;

    Page* page = this->page();
    if (!page)
        return;

    if (callStack)
        InspectorInstrumentation::addMessageToConsole(page, source, LogMessageType, level, message, callStack, requestIdentifier);
    else
        InspectorInstrumentation::addMessageToConsole(page, source, LogMessageType, level, message, url, lineNumber, state, requestIdentifier);

    if (!m_frame->settings() || m_frame->settings()->privateBrowsingEnabled())
        return;

    page->chrome()->client()->addMessageToConsole(source, level, message, lineNumber, url);

    if (!m_frame->settings()->logsPageMessagesToSystemConsoleEnabled() && !shouldPrintExceptions())
        return;

    printSourceURLAndLine(url, lineNumber);
    printMessageSourceAndLevelPrefix(source, level);

    printf(" %s\n", message.utf8().data());
}

static void internalAddMessage(Page* page, MessageType type, MessageLevel level, ScriptState* state, PassRefPtr<ScriptArguments> prpArguments, bool acceptNoArguments = false, bool printTrace = false)
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

    if (!page->settings() || page->settings()->privateBrowsingEnabled())
        return;

    if (gotMessage)
        page->chrome()->client()->addMessageToConsole(ConsoleAPIMessageSource, type, level, message, lastCaller.lineNumber(), lastCaller.sourceURL());

    if (!page->settings()->logsPageMessagesToSystemConsoleEnabled() && !Console::shouldPrintExceptions())
        return;

    printSourceURLAndLine(lastCaller.sourceURL(), lastCaller.lineNumber());
    printMessageSourceAndLevelPrefix(ConsoleAPIMessageSource, level);

    for (size_t i = 0; i < arguments->argumentCount(); ++i) {
        String argAsString = arguments->argumentAt(i).toString(arguments->globalState());
        printf(" %s", argAsString.utf8().data());
    }

    printf("\n");

    if (printTrace) {
        printf("Stack Trace\n");
        for (size_t i = 0; i < callStack->size(); ++i) {
            String functionName = String(callStack->at(i).functionName());
            printf("\t%s\n", functionName.utf8().data());
        }
    }
}

void Console::debug(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    // In Firebug, console.debug has the same behavior as console.log. So we'll do the same.
    log(state, arguments);
}

void Console::error(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), LogMessageType, ErrorMessageLevel, state, arguments);
}

void Console::info(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    log(state, arguments);
}

void Console::log(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), LogMessageType, LogMessageLevel, state, arguments);
}

void Console::warn(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), LogMessageType, WarningMessageLevel, state, arguments);
}

void Console::dir(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), DirMessageType, LogMessageLevel, state, arguments);
}

void Console::dirxml(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), DirXMLMessageType, LogMessageLevel, state, arguments);
}

void Console::clear(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), ClearMessageType, LogMessageLevel, state, arguments, true);
}

void Console::trace(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    internalAddMessage(page(), TraceMessageType, LogMessageLevel, state, arguments, true, true);
}

void Console::assertCondition(ScriptState* state, PassRefPtr<ScriptArguments> arguments, bool condition)
{
    if (condition)
        return;

    internalAddMessage(page(), AssertMessageType, ErrorMessageLevel, state, arguments, true);
}

void Console::count(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    InspectorInstrumentation::consoleCount(page(), state, arguments);
}

void Console::markTimeline(PassRefPtr<ScriptArguments> arguments)
{
    InspectorInstrumentation::consoleTimeStamp(m_frame, arguments);
}

#if ENABLE(JAVASCRIPT_DEBUGGER)

void Console::profile(const String& title, ScriptState* state)
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
    InspectorInstrumentation::addStartProfilingMessageToConsole(page, resolvedTitle, lastCaller.lineNumber(), lastCaller.sourceURL());
}

void Console::profileEnd(const String& title, ScriptState* state)
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

#endif

void Console::time(const String& title)
{
    InspectorInstrumentation::startConsoleTiming(m_frame, title);
#if PLATFORM(CHROMIUM)
    TRACE_EVENT_COPY_ASYNC_BEGIN0("webkit", title.utf8().data(), this);
#endif
}

void Console::timeEnd(ScriptState* state, const String& title)
{
#if PLATFORM(CHROMIUM)
    TRACE_EVENT_COPY_ASYNC_END0("webkit", title.utf8().data(), this);
#endif
    RefPtr<ScriptCallStack> callStack(createScriptCallStackForConsole(state));
    InspectorInstrumentation::stopConsoleTiming(m_frame, title, callStack.release());
}

void Console::timeStamp(PassRefPtr<ScriptArguments> arguments)
{
    InspectorInstrumentation::consoleTimeStamp(m_frame, arguments);
}

void Console::group(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    InspectorInstrumentation::addMessageToConsole(page(), ConsoleAPIMessageSource, StartGroupMessageType, LogMessageLevel, String(), state, arguments);
}

void Console::groupCollapsed(ScriptState* state, PassRefPtr<ScriptArguments> arguments)
{
    InspectorInstrumentation::addMessageToConsole(page(), ConsoleAPIMessageSource, StartGroupCollapsedMessageType, LogMessageLevel, String(), state, arguments);
}

void Console::groupEnd()
{
    InspectorInstrumentation::addMessageToConsole(page(), ConsoleAPIMessageSource, EndGroupMessageType, LogMessageLevel, String(), String(), 0);
}

// static
void Console::mute()
{
    muteCount++;
}

// static
void Console::unmute()
{
    ASSERT(muteCount > 0);
    muteCount--;
}

PassRefPtr<MemoryInfo> Console::memory() const
{
    // FIXME: Because we create a new object here each time,
    // console.memory !== console.memory, which seems wrong.
    return MemoryInfo::create(m_frame);
}

static bool printExceptions = false;

bool Console::shouldPrintExceptions()
{
    return printExceptions;
}

void Console::setShouldPrintExceptions(bool print)
{
    printExceptions = print;
}

Page* Console::page() const
{
    if (!m_frame)
        return 0;
    return m_frame->page();
}

} // namespace WebCore
