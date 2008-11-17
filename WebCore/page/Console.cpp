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

#include "ChromeClient.h"
#include "CString.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "InspectorController.h"
#include "JSDOMBinding.h"
#include "Page.h"
#include "PageGroup.h"
#include "PlatformString.h"
#include <runtime/ArgList.h>
#include <runtime/Completion.h>
#include <runtime/JSObject.h>
#include <interpreter/Interpreter.h>
#include <profiler/Profiler.h>
#include <stdio.h>

using namespace JSC;

namespace WebCore {

Console::Console(Frame* frame)
    : m_frame(frame)
{
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
        case CSSMessageSource:
            sourceString = "CSS";
            break;
        default:
            ASSERT_NOT_REACHED();
            // Fall thru.
        case OtherMessageSource:
            sourceString = "OTHER";
            break;
    }

    const char* levelString;
    switch (level) {
        case TipMessageLevel:
            levelString = "TIP";
            break;
        default:
            ASSERT_NOT_REACHED();
            // Fall thru.
        case LogMessageLevel:
            levelString = "LOG";
            break;
        case WarningMessageLevel:
            levelString = "WARN";
            break;
        case ErrorMessageLevel:
            levelString = "ERROR";
            break;
    }

    printf("%s %s:", sourceString, levelString);
}

static void printToStandardOut(MessageSource source, MessageLevel level, const String& message, const String& sourceURL, unsigned lineNumber)
{
    if (!Console::shouldPrintExceptions())
        return;

    printSourceURLAndLine(sourceURL, lineNumber);
    printMessageSourceAndLevelPrefix(source, level);

    printf(" %s\n", message.utf8().data());
}

static void printToStandardOut(MessageLevel level, ExecState* exec, const ArgList& args, const KURL& url)
{
    if (!Console::shouldPrintExceptions())
        return;

    printSourceURLAndLine(url.prettyURL(), 0);
    printMessageSourceAndLevelPrefix(JSMessageSource, level);

    for (size_t i = 0; i < args.size(); ++i) {
        UString argAsString = args.at(exec, i)->toString(exec);
        printf(" %s", argAsString.UTF8String().c_str());
    }

    printf("\n");
}

static inline void retrieveLastCaller(ExecState* exec, KURL& url, unsigned& lineNumber)
{
    int signedLineNumber;
    intptr_t sourceID;
    UString urlString;
    JSValue* function;

    exec->interpreter()->retrieveLastCaller(exec, signedLineNumber, sourceID, urlString, function);

    url = KURL(urlString);
    lineNumber = (signedLineNumber >= 0 ? signedLineNumber : 0);
}

void Console::addMessage(MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceURL)
{
    Page* page = this->page();
    if (!page)
        return;

    if (source == JSMessageSource)
        page->chrome()->client()->addMessageToConsole(message, lineNumber, sourceURL);

    page->inspectorController()->addMessageToConsole(source, level, message, lineNumber, sourceURL);

    printToStandardOut(source, level, message, sourceURL, lineNumber);
}

void Console::debug(ExecState* exec, const ArgList& args)
{
    // In Firebug, console.debug has the same behavior as console.log. So we'll do the same.
    log(exec, args);
}

void Console::error(ExecState* exec, const ArgList& args)
{
    if (args.isEmpty())
        return;

    Page* page = this->page();
    if (!page)
        return;

    String message = args.at(exec, 0)->toString(exec);

    KURL url;
    unsigned lineNumber;
    retrieveLastCaller(exec, url, lineNumber);

    page->chrome()->client()->addMessageToConsole(message, lineNumber, url.prettyURL());
    page->inspectorController()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, exec, args, lineNumber, url.string());

    printToStandardOut(ErrorMessageLevel, exec, args, url);
}

void Console::info(ExecState* exec, const ArgList& args)
{
    if (args.isEmpty())
        return;

    Page* page = this->page();
    if (!page)
        return;

    String message = args.at(exec, 0)->toString(exec);

    KURL url;
    unsigned lineNumber;
    retrieveLastCaller(exec, url, lineNumber);

    page->chrome()->client()->addMessageToConsole(message, lineNumber, url.prettyURL());
    page->inspectorController()->addMessageToConsole(JSMessageSource, LogMessageLevel, exec, args, lineNumber, url.string());

    printToStandardOut(LogMessageLevel, exec, args, url);
}

void Console::log(ExecState* exec, const ArgList& args)
{
    if (args.isEmpty())
        return;

    Page* page = this->page();
    if (!page)
        return;

    String message = args.at(exec, 0)->toString(exec);

    KURL url;
    unsigned lineNumber;
    retrieveLastCaller(exec, url, lineNumber);

    page->chrome()->client()->addMessageToConsole(message, lineNumber, url.prettyURL());
    page->inspectorController()->addMessageToConsole(JSMessageSource, LogMessageLevel, exec, args, lineNumber, url.string());

    printToStandardOut(LogMessageLevel, exec, args, url);
}

void Console::dir(ExecState* exec, const ArgList& args)
{
    if (args.isEmpty())
        return;

    Page* page = this->page();
    if (!page)
        return;

    page->inspectorController()->addMessageToConsole(JSMessageSource, ObjectMessageLevel, exec, args, 0, String());
}

void Console::dirxml(ExecState* exec, const ArgList& args)
{
    if (args.isEmpty())
        return;

    Page* page = this->page();
    if (!page)
        return;

    page->inspectorController()->addMessageToConsole(JSMessageSource, NodeMessageLevel, exec, args, 0, String());
}

void Console::trace(ExecState* exec)
{
    Page* page = this->page();
    if (!page)
        return;

    int signedLineNumber;
    intptr_t sourceID;
    UString urlString;
    JSValue* func;

    exec->interpreter()->retrieveLastCaller(exec, signedLineNumber, sourceID, urlString, func);

    ArgList args;
    while (!func->isNull()) {
        args.append(func);
        func = exec->interpreter()->retrieveCaller(exec, asInternalFunction(func));
    }
    
    page->inspectorController()->addMessageToConsole(JSMessageSource, TraceMessageLevel, exec, args, 0, String());
}

void Console::assertCondition(bool condition, ExecState* exec, const ArgList& args)
{
    if (condition)
        return;

    Page* page = this->page();
    if (!page)
        return;

    KURL url;
    unsigned lineNumber;
    retrieveLastCaller(exec, url, lineNumber);

    // FIXME: <https://bugs.webkit.org/show_bug.cgi?id=19135> It would be nice to prefix assertion failures with a message like "Assertion failed: ".
    // FIXME: <https://bugs.webkit.org/show_bug.cgi?id=19136> We should print a message even when args.isEmpty() is true.

    page->inspectorController()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, exec, args, lineNumber, url.string());

    printToStandardOut(ErrorMessageLevel, exec, args, url);
}

void Console::count(ExecState* exec, const ArgList& args)
{
    Page* page = this->page();
    if (!page)
        return;

    KURL url;
    unsigned lineNumber;
    retrieveLastCaller(exec, url, lineNumber);

    UString title;
    if (args.size() >= 1)
        title = valueToStringWithUndefinedOrNullCheck(exec, args.at(exec, 0));

    page->inspectorController()->count(title, lineNumber, url.string());
}

void Console::profile(ExecState* exec, const ArgList& args)
{
    Page* page = this->page();
    if (!page)
        return;

    // FIXME: log a console message when profiling is disabled.
    if (!page->inspectorController()->profilerEnabled())
        return;

    UString title = args.at(exec, 0)->toString(exec);
    Profiler::profiler()->startProfiling(exec, title);
}

void Console::profileEnd(ExecState* exec, const ArgList& args)
{
    Page* page = this->page();
    if (!page)
        return;

    if (!page->inspectorController()->profilerEnabled())
        return;

    UString title;
    if (args.size() >= 1)
        title = valueToStringWithUndefinedOrNullCheck(exec, args.at(exec, 0));

    RefPtr<Profile> profile = Profiler::profiler()->stopProfiling(exec, title);
    if (!profile)
        return;

    m_profiles.append(profile);

    if (Page* page = this->page()) {
        KURL url;
        unsigned lineNumber;
        retrieveLastCaller(exec, url, lineNumber);

        page->inspectorController()->addProfile(profile, lineNumber, url);
    }
}

void Console::time(const UString& title)
{
    if (title.isNull())
        return;
    
    Page* page = this->page();
    if (!page)
        return;
    
    page->inspectorController()->startTiming(title);
}

void Console::timeEnd(ExecState* exec, const ArgList& args)
{
    UString title;
    if (args.size() >= 1)
        title = valueToStringWithUndefinedOrNullCheck(exec, args.at(exec, 0));
    if (title.isNull())
        return;

    Page* page = this->page();
    if (!page)
        return;

    double elapsed;
    if (!page->inspectorController()->stopTiming(title, elapsed))
        return;

    String message = String(title) + String::format(": %.0fms", elapsed);

    KURL url;
    unsigned lineNumber;
    retrieveLastCaller(exec, url, lineNumber);

    page->inspectorController()->addMessageToConsole(JSMessageSource, LogMessageLevel, message, lineNumber, url.string());
}

void Console::group(ExecState* exec, const ArgList& arguments)
{
    Page* page = this->page();
    if (!page)
        return;

    page->inspectorController()->startGroup(JSMessageSource, exec, arguments, 0, String());
}

void Console::groupEnd()
{
    Page* page = this->page();
    if (!page)
        return;

    page->inspectorController()->endGroup(JSMessageSource, 0, String());
}

void Console::warn(ExecState* exec, const ArgList& args)
{
    if (args.isEmpty())
        return;

    Page* page = this->page();
    if (!page)
        return;

    String message = args.at(exec, 0)->toString(exec);

    KURL url;
    unsigned lineNumber;
    retrieveLastCaller(exec, url, lineNumber);

    page->chrome()->client()->addMessageToConsole(message, lineNumber, url.prettyURL());
    page->inspectorController()->addMessageToConsole(JSMessageSource, WarningMessageLevel, exec, args, lineNumber, url.string());

    printToStandardOut(WarningMessageLevel, exec, args, url);
}

void Console::reportException(ExecState* exec, JSValue* exception)
{
    UString errorMessage = exception->toString(exec);
    JSObject* exceptionObject = exception->toObject(exec);
    int lineNumber = exceptionObject->get(exec, Identifier(exec, "line"))->toInt32(exec);
    UString exceptionSourceURL = exceptionObject->get(exec, Identifier(exec, "sourceURL"))->toString(exec);
    addMessage(JSMessageSource, ErrorMessageLevel, errorMessage, lineNumber, exceptionSourceURL);
    if (exec->hadException())
        exec->clearException();
}

void Console::reportCurrentException(ExecState* exec)
{
    JSValue* exception = exec->exception();
    exec->clearException();
    reportException(exec, exception);
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
