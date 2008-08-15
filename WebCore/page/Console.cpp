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
#include "Page.h"
#include "PageGroup.h"
#include "PlatformString.h"
#include <kjs/ArgList.h>
#include <kjs/interpreter.h>
#include <kjs/JSObject.h>
#include <profiler/Profile.h>
#include <stdio.h>

using namespace KJS;

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
    if (!Interpreter::shouldPrintExceptions())
        return;

    printSourceURLAndLine(sourceURL, lineNumber);
    printMessageSourceAndLevelPrefix(source, level);

    printf(" %s\n", message.utf8().data());
}

static void printToStandardOut(MessageLevel level, ExecState* exec, const ArgList& args, const KURL& url)
{
    if (!Interpreter::shouldPrintExceptions())
        return;

    printSourceURLAndLine(url.prettyURL(), 0);
    printMessageSourceAndLevelPrefix(JSMessageSource, level);

    for (size_t i = 0; i < args.size(); ++i) {
        UString argAsString = args.at(exec, i)->toString(exec);
        printf(" %s", argAsString.UTF8String().c_str());
    }

    printf("\n");
}

void Console::addMessage(MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceURL)
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
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

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    String message = args.at(exec, 0)->toString(exec);
    const KURL& url = m_frame->loader()->url();
    String prettyURL = url.prettyURL();

    page->chrome()->client()->addMessageToConsole(message, 0, prettyURL);
    page->inspectorController()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, exec, args, 0, url.string());

    printToStandardOut(ErrorMessageLevel, exec, args, url);
}

void Console::info(ExecState* exec, const ArgList& args)
{
    if (args.isEmpty())
        return;

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    String message = args.at(exec, 0)->toString(exec);
    const KURL& url = m_frame->loader()->url();
    String prettyURL = url.prettyURL();

    page->chrome()->client()->addMessageToConsole(message, 0, prettyURL);
    page->inspectorController()->addMessageToConsole(JSMessageSource, LogMessageLevel, exec, args, 0, url.string());

    printToStandardOut(LogMessageLevel, exec, args, url);
}

void Console::log(ExecState* exec, const ArgList& args)
{
    if (args.isEmpty())
        return;

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    String message = args.at(exec, 0)->toString(exec);
    const KURL& url = m_frame->loader()->url();
    String prettyURL = url.prettyURL();

    page->chrome()->client()->addMessageToConsole(message, 0, prettyURL);
    page->inspectorController()->addMessageToConsole(JSMessageSource, LogMessageLevel, exec, args, 0, url.string());

    printToStandardOut(LogMessageLevel, exec, args, url);
}

void Console::dir(ExecState* exec, const ArgList& args)
{
    if (args.isEmpty())
        return;

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    page->inspectorController()->addMessageToConsole(JSMessageSource, ObjectMessageLevel, exec, args, 0, String());
}

void Console::assertCondition(bool condition, ExecState* exec, const ArgList& args)
{
    if (condition)
        return;

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    const KURL& url = m_frame->loader()->url();

    // FIXME: <https://bugs.webkit.org/show_bug.cgi?id=19135> It would be nice to prefix assertion failures with a message like "Assertion failed: ".
    // FIXME: <https://bugs.webkit.org/show_bug.cgi?id=19136> We should print a message even when args.isEmpty() is true.

    page->inspectorController()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, exec, args, 0, url.string());

    printToStandardOut(ErrorMessageLevel, exec, args, url);
}

void Console::profile(ExecState* exec, const ArgList& args)
{
    UString title = args.at(exec, 0)->toString(exec);
    Profiler::profiler()->startProfiling(exec, title, this);
}

void Console::profileEnd(ExecState* exec, const ArgList& args)
{
    UString title;
    if (args.size() >= 1)
        title = args.at(exec, 0)->toString(exec);

    Profiler::profiler()->stopProfiling(exec, title);
}

void Console::time(const UString& title)
{
    if (title.isNull())
        return;
    
    if (!m_frame)
        return;
    
    Page* page = m_frame->page();
    if (!page)
        return;
    
    page->inspectorController()->startTiming(title);
}

void Console::timeEnd(const UString& title)
{
    if (title.isNull())
        return;
    
    if (!m_frame)
        return;
    
    Page* page = m_frame->page();
    if (!page)
        return;
    
    double elapsed;
    if (!page->inspectorController()->stopTiming(title, elapsed))
        return;
    
    String message = String(title) + String::format(": %.0fms", elapsed);
    // FIXME: <https://bugs.webkit.org/show_bug.cgi?id=19791> We should pass in the real sourceURL here so that the Inspector can show it.
    page->inspectorController()->addMessageToConsole(JSMessageSource, LogMessageLevel, message, 0, String());
}

void Console::group(ExecState* exec, const ArgList& arguments)
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    page->inspectorController()->startGroup(JSMessageSource, exec, arguments, 0, String());
}

void Console::groupEnd()
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    page->inspectorController()->endGroup(JSMessageSource, 0, String());
}

void Console::finishedProfiling(PassRefPtr<Profile> prpProfile)
{
    if (!m_frame)
        return;

    if (Page* page = m_frame->page())
        page->inspectorController()->addProfile(prpProfile);
}

void Console::warn(ExecState* exec, const ArgList& args)
{
    if (args.isEmpty())
        return;

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    String message = args.at(exec, 0)->toString(exec);
    const KURL& url = m_frame->loader()->url();
    String prettyURL = url.prettyURL();

    page->chrome()->client()->addMessageToConsole(message, 0, prettyURL);
    page->inspectorController()->addMessageToConsole(JSMessageSource, WarningMessageLevel, exec, args, 0, url.string());

    printToStandardOut(WarningMessageLevel, exec, args, url);
}

void Console::reportException(ExecState* exec, JSValue* exception)
{
    UString errorMessage = exception->toString(exec);
    JSObject* exceptionObject = exception->toObject(exec);
    int lineNumber = exceptionObject->get(exec, Identifier(exec, "line"))->toInt32(exec);
    UString exceptionSourceURL = exceptionObject->get(exec, Identifier(exec, "sourceURL"))->toString(exec);
    addMessage(JSMessageSource, ErrorMessageLevel, errorMessage, lineNumber, exceptionSourceURL);
}

void Console::reportCurrentException(ExecState* exec)
{
    JSValue* exception = exec->exception();
    exec->clearException();
    reportException(exec, exception);
}

} // namespace WebCore
