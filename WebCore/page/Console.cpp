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

#include "CString.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ConsoleMessage.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "InspectorController.h"
#include "Page.h"
#include "PageGroup.h"
#include "PlatformString.h"

#include "ScriptCallStack.h"
#include "ScriptProfile.h"
#include "ScriptProfiler.h"
#include <stdio.h>
#include <wtf/UnusedParam.h>

namespace WebCore {

Console::Console(Frame* frame)
    : m_frame(frame)
{
}

Frame* Console::frame() const
{
    return m_frame;
}

void Console::disconnectFrame()
{
    m_frame = 0;
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

static bool getFirstArgumentAsString(ScriptState* scriptState, const ScriptCallFrame& callFrame, String& result, bool checkForNullOrUndefined = false)
{
    if (!callFrame.argumentCount())
        return false;

    const ScriptValue& value = callFrame.argumentAt(0);
    if (checkForNullOrUndefined && (value.isNull() || value.isUndefined()))
        return false;

    result = value.toString(scriptState);
    return true;
}

static void printMessageSourceAndLevelPrefix(MessageSource source, MessageLevel level)
{
    const char* sourceString;
    switch (source) {
    case HTMLMessageSource:
        sourceString = "HTML";
        break;
    case WMLMessageSource:
        sourceString = "WML";
        break;
    case XMLMessageSource:
        sourceString = "XML";
        break;
    case JSMessageSource:
        sourceString = "JS";
        break;
    case CSSMessageSource:
        sourceString = "CSS";
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

void Console::addMessage(MessageSource source, MessageType type, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceURL)
{
    Page* page = this->page();
    if (!page)
        return;

    if (source == JSMessageSource)
        page->chrome()->client()->addMessageToConsole(source, type, level, message, lineNumber, sourceURL);

#if ENABLE(INSPECTOR)
    page->inspectorController()->addMessageToConsole(source, type, level, message, lineNumber, sourceURL);
#endif

    if (!Console::shouldPrintExceptions())
        return;

    printSourceURLAndLine(sourceURL, lineNumber);
    printMessageSourceAndLevelPrefix(source, level);

    printf(" %s\n", message.utf8().data());
}

void Console::addMessage(MessageType type, MessageLevel level, ScriptCallStack* callStack, bool acceptNoArguments)
{
    Page* page = this->page();
    if (!page)
        return;

    const ScriptCallFrame& lastCaller = callStack->at(0);

    if (!acceptNoArguments && !lastCaller.argumentCount())
        return;

    String message;
    if (getFirstArgumentAsString(callStack->state(), lastCaller, message))
        page->chrome()->client()->addMessageToConsole(JSMessageSource, type, level, message, lastCaller.lineNumber(), lastCaller.sourceURL().prettyURL());

#if ENABLE(INSPECTOR)
    page->inspectorController()->addMessageToConsole(JSMessageSource, type, level, callStack);
#endif

    if (!Console::shouldPrintExceptions())
        return;

    printSourceURLAndLine(lastCaller.sourceURL().prettyURL(), 0);
    printMessageSourceAndLevelPrefix(JSMessageSource, level);

    for (unsigned i = 0; i < lastCaller.argumentCount(); ++i) {
        String argAsString;
        if (lastCaller.argumentAt(i).getString(callStack->state(), argAsString))
            printf(" %s", argAsString.utf8().data());
    }
    printf("\n");
}

void Console::debug(ScriptCallStack* callStack)
{
    // In Firebug, console.debug has the same behavior as console.log. So we'll do the same.
    log(callStack);
}

void Console::error(ScriptCallStack* callStack)
{
    addMessage(LogMessageType, ErrorMessageLevel, callStack);
}

void Console::info(ScriptCallStack* callStack)
{
    log(callStack);
}

void Console::log(ScriptCallStack* callStack)
{
    addMessage(LogMessageType, LogMessageLevel, callStack);
}

void Console::dir(ScriptCallStack* callStack)
{
    addMessage(ObjectMessageType, LogMessageLevel, callStack);
}

void Console::dirxml(ScriptCallStack* callStack)
{
    // The standard behavior of our console.log will print the DOM tree for nodes.
    log(callStack);
}

void Console::trace(ScriptCallStack* callStack)
{
    addMessage(TraceMessageType, LogMessageLevel, callStack, true);

    if (!shouldPrintExceptions())
        return;

    printf("Stack Trace\n");
    for (unsigned i = 0; i < callStack->size(); ++i) {
        String functionName = String(callStack->at(i).functionName());
        printf("\t%s\n", functionName.utf8().data());
    }
}

void Console::assertCondition(bool condition, ScriptCallStack* callStack)
{
    if (condition)
        return;

    addMessage(AssertMessageType, ErrorMessageLevel, callStack, true);
}

void Console::count(ScriptCallStack* callStack)
{
#if ENABLE(INSPECTOR)
    Page* page = this->page();
    if (!page)
        return;

    const ScriptCallFrame& lastCaller = callStack->at(0);
    // Follow Firebug's behavior of counting with null and undefined title in
    // the same bucket as no argument
    String title;
    getFirstArgumentAsString(callStack->state(), lastCaller, title);

    page->inspectorController()->count(title, lastCaller.lineNumber(), lastCaller.sourceURL().string());
#else
    UNUSED_PARAM(callStack);
#endif
}

void Console::markTimeline(ScriptCallStack* callStack)
{
#if ENABLE(INSPECTOR)
    Page* page = this->page();
    if (!page)
        return;

    const ScriptCallFrame& lastCaller = callStack->at(0);
    String message;
    getFirstArgumentAsString(callStack->state(), lastCaller, message);

    page->inspectorController()->markTimeline(message);
#else
    UNUSED_PARAM(callStack);
#endif
}

#if ENABLE(WML)
String Console::lastWMLErrorMessage() const
{
    Page* page = this->page();
    if (!page)
        return String();

    const Vector<ConsoleMessage*>& consoleMessages = page->inspectorController()->consoleMessages();
    if (consoleMessages.isEmpty())
        return String();

    Vector<ConsoleMessage*>::const_iterator it = consoleMessages.begin();
    const Vector<ConsoleMessage*>::const_iterator end = consoleMessages.end();

    for (; it != end; ++it) {
        ConsoleMessage* message = *it;
        if (message->source() != WMLMessageSource)
            continue;

        return message->message();
    }

    return String();
}
#endif

#if ENABLE(JAVASCRIPT_DEBUGGER)

void Console::profile(const String& title, ScriptCallStack* callStack)
{
    Page* page = this->page();
    if (!page)
        return;

#if ENABLE(INSPECTOR)
    InspectorController* controller = page->inspectorController();
    // FIXME: log a console message when profiling is disabled.
    if (!controller->profilerEnabled())
        return;
#endif

    String resolvedTitle = title;
    if (title.isNull()) // no title so give it the next user initiated profile title.
#if ENABLE(INSPECTOR)
        resolvedTitle = controller->getCurrentUserInitiatedProfileName(true);
#else
        resolvedTitle = "";
#endif

    ScriptProfiler::start(callStack->state(), resolvedTitle);

#if ENABLE(INSPECTOR)
    const ScriptCallFrame& lastCaller = callStack->at(0);
    controller->addStartProfilingMessageToConsole(resolvedTitle, lastCaller.lineNumber(), lastCaller.sourceURL());
#endif
}

void Console::profileEnd(const String& title, ScriptCallStack* callStack)
{
    Page* page = this->page();
    if (!page)
        return;

#if ENABLE(INSPECTOR)
    InspectorController* controller = page->inspectorController();
    if (!controller->profilerEnabled())
        return;
#endif

    RefPtr<ScriptProfile> profile = ScriptProfiler::stop(callStack->state(), title);
    if (!profile)
        return;

    m_profiles.append(profile);

#if ENABLE(INSPECTOR)
    const ScriptCallFrame& lastCaller = callStack->at(0);
    controller->addProfile(profile, lastCaller.lineNumber(), lastCaller.sourceURL());
#endif
}

#endif

void Console::time(const String& title)
{
#if ENABLE(INSPECTOR)
    Page* page = this->page();
    if (!page)
        return;

    // Follow Firebug's behavior of requiring a title that is not null or
    // undefined for timing functions
    if (title.isNull())
        return;

    page->inspectorController()->startTiming(title);
#else
    UNUSED_PARAM(title);
#endif
}

void Console::timeEnd(const String& title, ScriptCallStack* callStack)
{
#if ENABLE(INSPECTOR)
    Page* page = this->page();
    if (!page)
        return;

    // Follow Firebug's behavior of requiring a title that is not null or
    // undefined for timing functions
    if (title.isNull())
        return;

    double elapsed;
    if (!page->inspectorController()->stopTiming(title, elapsed))
        return;

    String message = title + String::format(": %.0fms", elapsed);

    const ScriptCallFrame& lastCaller = callStack->at(0);
    page->inspectorController()->addMessageToConsole(JSMessageSource, LogMessageType, LogMessageLevel, message, lastCaller.lineNumber(), lastCaller.sourceURL().string());
#else
    UNUSED_PARAM(title);
    UNUSED_PARAM(callStack);
#endif
}

void Console::group(ScriptCallStack* callStack)
{
#if ENABLE(INSPECTOR)
    Page* page = this->page();
    if (!page)
        return;

    page->inspectorController()->startGroup(JSMessageSource, callStack);
#else
    UNUSED_PARAM(callStack);
#endif
}

void Console::groupEnd()
{
#if ENABLE(INSPECTOR)
    Page* page = this->page();
    if (!page)
        return;

    page->inspectorController()->endGroup(JSMessageSource, 0, String());
#endif
}

void Console::warn(ScriptCallStack* callStack)
{
    addMessage(LogMessageType, WarningMessageLevel, callStack);
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
