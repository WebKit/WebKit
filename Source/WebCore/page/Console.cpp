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
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "InspectorConsoleInstrumentation.h"
#include "InspectorController.h"
#include "MemoryInfo.h"
#include "Page.h"
#include "PageGroup.h"
#include "PlatformString.h"
#include "ScriptArguments.h"
#include "ScriptCallStack.h"
#include "ScriptProfile.h"
#include "ScriptProfiler.h"
#include "ScriptValue.h"
#include <stdio.h>
#include <wtf/UnusedParam.h>
#include <wtf/text/CString.h>

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

void Console::addMessage(MessageSource source, MessageType type, MessageLevel level, const String& message, PassRefPtr<ScriptCallStack> callStack)
{
    addMessage(source, type, level, message, String(), 0, callStack);
}

void Console::addMessage(MessageSource source, MessageType type, MessageLevel level, const String& message, const String& sourceURL, unsigned lineNumber, PassRefPtr<ScriptCallStack> callStack)
{

    if (muteCount && source != ConsoleAPIMessageSource)
        return;

    Page* page = this->page();
    if (!page)
        return;

    page->chrome()->client()->addMessageToConsole(source, type, level, message, lineNumber, sourceURL);

    if (callStack)
        InspectorInstrumentation::addMessageToConsole(page, source, type, level, message, 0, callStack);
    else
        InspectorInstrumentation::addMessageToConsole(page, source, type, level, message, sourceURL, lineNumber);

    if (!Console::shouldPrintExceptions())
        return;

    printSourceURLAndLine(sourceURL, lineNumber);
    printMessageSourceAndLevelPrefix(source, level);

    printf(" %s\n", message.utf8().data());
}

void Console::addMessage(MessageType type, MessageLevel level, PassRefPtr<ScriptArguments> prpArguments,  PassRefPtr<ScriptCallStack> prpCallStack, bool acceptNoArguments)
{
    RefPtr<ScriptArguments> arguments = prpArguments;
    RefPtr<ScriptCallStack> callStack = prpCallStack;

    Page* page = this->page();
    if (!page)
        return;

    const ScriptCallFrame& lastCaller = callStack->at(0);

    if (!acceptNoArguments && !arguments->argumentCount())
        return;

    if (Console::shouldPrintExceptions()) {
        printSourceURLAndLine(lastCaller.sourceURL(), 0);
        printMessageSourceAndLevelPrefix(ConsoleAPIMessageSource, level);

        for (unsigned i = 0; i < arguments->argumentCount(); ++i) {
            String argAsString;
            if (arguments->argumentAt(i).getString(arguments->globalState(), argAsString))
                printf(" %s", argAsString.utf8().data());
        }
        printf("\n");
    }

    String message;
    if (arguments->getFirstArgumentAsString(message))
        page->chrome()->client()->addMessageToConsole(ConsoleAPIMessageSource, type, level, message, lastCaller.lineNumber(), lastCaller.sourceURL());

    InspectorInstrumentation::addMessageToConsole(page, ConsoleAPIMessageSource, type, level, message, arguments, callStack);
}

void Console::debug(PassRefPtr<ScriptArguments> arguments, PassRefPtr<ScriptCallStack> callStack)
{
    // In Firebug, console.debug has the same behavior as console.log. So we'll do the same.
    log(arguments, callStack);
}

void Console::error(PassRefPtr<ScriptArguments> arguments, PassRefPtr<ScriptCallStack> callStack)
{
    addMessage(LogMessageType, ErrorMessageLevel, arguments, callStack);
}

void Console::info(PassRefPtr<ScriptArguments> arguments, PassRefPtr<ScriptCallStack> callStack)
{
    log(arguments, callStack);
}

void Console::log(PassRefPtr<ScriptArguments> arguments, PassRefPtr<ScriptCallStack> callStack)
{
    addMessage(LogMessageType, LogMessageLevel, arguments, callStack);
}

void Console::warn(PassRefPtr<ScriptArguments> arguments, PassRefPtr<ScriptCallStack> callStack)
{
    addMessage(LogMessageType, WarningMessageLevel, arguments, callStack);
}

void Console::dir(PassRefPtr<ScriptArguments> arguments, PassRefPtr<ScriptCallStack> callStack)
{
    addMessage(DirMessageType, LogMessageLevel, arguments, callStack);
}

void Console::dirxml(PassRefPtr<ScriptArguments> arguments, PassRefPtr<ScriptCallStack> callStack)
{
    addMessage(DirXMLMessageType, LogMessageLevel, arguments, callStack);
}

void Console::trace(PassRefPtr<ScriptArguments> arguments, PassRefPtr<ScriptCallStack> prpCallStack)
{
    RefPtr<ScriptCallStack> callStack = prpCallStack;
    addMessage(TraceMessageType, LogMessageLevel, arguments, callStack, true);

    if (!shouldPrintExceptions())
        return;

    printf("Stack Trace\n");
    for (unsigned i = 0; i < callStack->size(); ++i) {
        String functionName = String(callStack->at(i).functionName());
        printf("\t%s\n", functionName.utf8().data());
    }
}

void Console::assertCondition(PassRefPtr<ScriptArguments> arguments, PassRefPtr<ScriptCallStack> callStack, bool condition)
{
    if (condition)
        return;

    addMessage(AssertMessageType, ErrorMessageLevel, arguments, callStack, true);
}

void Console::count(PassRefPtr<ScriptArguments> arguments, PassRefPtr<ScriptCallStack> callStack)
{
    InspectorInstrumentation::consoleCount(page(), arguments, callStack);
}

void Console::markTimeline(PassRefPtr<ScriptArguments> arguments, PassRefPtr<ScriptCallStack>)
{
    InspectorInstrumentation::consoleTimeStamp(m_frame, arguments);
}

#if ENABLE(JAVASCRIPT_DEBUGGER)

void Console::profile(const String& title, ScriptState* state, PassRefPtr<ScriptCallStack> callStack)
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

    const ScriptCallFrame& lastCaller = callStack->at(0);
    InspectorInstrumentation::addStartProfilingMessageToConsole(page, resolvedTitle, lastCaller.lineNumber(), lastCaller.sourceURL());
}

void Console::profileEnd(const String& title, ScriptState* state, PassRefPtr<ScriptCallStack> callStack)
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

void Console::timeEnd(PassRefPtr<ScriptArguments>, PassRefPtr<ScriptCallStack> callStack, const String& title)
{
#if PLATFORM(CHROMIUM)
    TRACE_EVENT_COPY_ASYNC_END0("webkit", title.utf8().data(), this);
#endif
    InspectorInstrumentation::stopConsoleTiming(m_frame, title, callStack);
}

void Console::timeStamp(PassRefPtr<ScriptArguments> arguments, PassRefPtr<ScriptCallStack>)
{
    InspectorInstrumentation::consoleTimeStamp(m_frame, arguments);
}

void Console::group(PassRefPtr<ScriptArguments> arguments, PassRefPtr<ScriptCallStack> callStack)
{
    InspectorInstrumentation::addMessageToConsole(page(), ConsoleAPIMessageSource, StartGroupMessageType, LogMessageLevel, String(), arguments, callStack);
}

void Console::groupCollapsed(PassRefPtr<ScriptArguments> arguments, PassRefPtr<ScriptCallStack> callStack)
{
    InspectorInstrumentation::addMessageToConsole(page(), ConsoleAPIMessageSource, StartGroupCollapsedMessageType, LogMessageLevel, String(), arguments, callStack);
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
