/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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
#include "PageConsole.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "Frame.h"
#include "InspectorConsoleInstrumentation.h"
#include "InspectorController.h"
#include "JSMainThreadExecState.h"
#include "MainFrame.h"
#include "Page.h"
#include "ScriptProfile.h"
#include "ScriptProfiler.h"
#include "ScriptableDocumentParser.h"
#include "Settings.h"
#include <bindings/ScriptValue.h>
#include <inspector/ScriptArguments.h>
#include <inspector/ScriptCallStack.h>
#include <inspector/ScriptCallStackFactory.h>

using namespace Inspector;

namespace WebCore {

PageConsole::PageConsole(Page& page)
    : m_page(page)
{
}

PageConsole::~PageConsole()
{
}

static int muteCount = 0;
static bool printExceptions = false;

bool PageConsole::shouldPrintExceptions()
{
    return printExceptions;
}

void PageConsole::setShouldPrintExceptions(bool print)
{
    printExceptions = print;
}

void PageConsole::mute()
{
    muteCount++;
}

void PageConsole::unmute()
{
    ASSERT(muteCount > 0);
    muteCount--;
}

void PageConsole::addMessage(MessageSource source, MessageLevel level, const String& message, unsigned long requestIdentifier, Document* document)
{
    String url;
    if (document)
        url = document->url().string();

    // FIXME: The below code attempts to determine line numbers for parser generated errors, but this is not the only reason why we can get here.
    // For example, if we are still parsing and get a WebSocket network error, it will be erroneously attributed to a line where parsing was paused.
    // Also, we should determine line numbers for script generated messages (e.g. calling getImageData on a canvas).
    // We probably need to split this function into multiple ones, as appropriate for different call sites. Or maybe decide based on MessageSource.
    // https://bugs.webkit.org/show_bug.cgi?id=125340
    unsigned line = 0;
    unsigned column = 0;
    if (document && document->parsing() && !document->isInDocumentWrite() && document->scriptableDocumentParser()) {
        ScriptableDocumentParser* parser = document->scriptableDocumentParser();
        if (!parser->isWaitingForScripts() && !JSMainThreadExecState::currentState()) {
            TextPosition position = parser->textPosition();
            line = position.m_line.oneBasedInt();
            column = position.m_column.oneBasedInt();
        }
    }
    addMessage(source, level, message, url, line, column, 0, JSMainThreadExecState::currentState(), requestIdentifier);
}

void PageConsole::addMessage(MessageSource source, MessageLevel level, const String& message, PassRefPtr<ScriptCallStack> callStack)
{
    addMessage(source, level, message, String(), 0, 0, callStack, 0);
}

void PageConsole::addMessage(MessageSource source, MessageLevel level, const String& message, const String& url, unsigned lineNumber, unsigned columnNumber, PassRefPtr<ScriptCallStack> callStack, JSC::ExecState* state, unsigned long requestIdentifier)
{
    if (muteCount && source != MessageSource::ConsoleAPI)
        return;

    if (callStack)
        InspectorInstrumentation::addMessageToConsole(&m_page, source, MessageType::Log, level, message, callStack, requestIdentifier);
    else
        InspectorInstrumentation::addMessageToConsole(&m_page, source, MessageType::Log, level, message, url, lineNumber, columnNumber, state, requestIdentifier);

    if (source == MessageSource::CSS)
        return;

    if (m_page.settings().privateBrowsingEnabled())
        return;

    m_page.chrome().client().addMessageToConsole(source, level, message, lineNumber, columnNumber, url);

    if (!m_page.settings().logsPageMessagesToSystemConsoleEnabled() && !shouldPrintExceptions())
        return;

    ConsoleClient::printConsoleMessage(MessageSource::ConsoleAPI, MessageType::Log, level, message, url, lineNumber, columnNumber);
}


void PageConsole::messageWithTypeAndLevel(MessageType type, MessageLevel level, JSC::ExecState* exec, PassRefPtr<Inspector::ScriptArguments> prpArguments)
{
    RefPtr<ScriptArguments> arguments = prpArguments;

    String message;
    bool gotMessage = arguments->getFirstArgumentAsString(message);
    InspectorInstrumentation::addMessageToConsole(&m_page, MessageSource::ConsoleAPI, type, level, message, exec, arguments);

    if (m_page.settings().privateBrowsingEnabled())
        return;

    if (gotMessage) {
        size_t stackSize = type == MessageType::Trace ? ScriptCallStack::maxCallStackSizeToCapture : 1;
        RefPtr<ScriptCallStack> callStack(createScriptCallStackForConsole(exec, stackSize));
        const ScriptCallFrame& lastCaller = callStack->at(0);
        m_page.chrome().client().addMessageToConsole(MessageSource::ConsoleAPI, type, level, message, lastCaller.lineNumber(), lastCaller.columnNumber(), lastCaller.sourceURL());
    }

    if (m_page.settings().logsPageMessagesToSystemConsoleEnabled() || PageConsole::shouldPrintExceptions())
        ConsoleClient::printConsoleMessageWithArguments(MessageSource::ConsoleAPI, type, level, exec, arguments.release());
}

void PageConsole::count(JSC::ExecState* exec, PassRefPtr<ScriptArguments> arguments)
{
    InspectorInstrumentation::consoleCount(&m_page, exec, arguments);
}

void PageConsole::profile(JSC::ExecState* exec, const String& title)
{
    // FIXME: log a console message when profiling is disabled.
    if (!InspectorInstrumentation::profilerEnabled(&m_page))
        return;

    // If no title is given, build the next user initiated profile title.
    String resolvedTitle = title;
    if (title.isNull())
        resolvedTitle = InspectorInstrumentation::getCurrentUserInitiatedProfileName(&m_page, true);

    ScriptProfiler::start(exec, resolvedTitle);

    RefPtr<ScriptCallStack> callStack(createScriptCallStackForConsole(exec, 1));
    const ScriptCallFrame& lastCaller = callStack->at(0);
    InspectorInstrumentation::addStartProfilingMessageToConsole(&m_page, resolvedTitle, lastCaller.lineNumber(), lastCaller.columnNumber(), lastCaller.sourceURL());
}

void PageConsole::profileEnd(JSC::ExecState* exec, const String& title)
{
    if (!InspectorInstrumentation::profilerEnabled(&m_page))
        return;

    RefPtr<ScriptProfile> profile = ScriptProfiler::stop(exec, title);
    if (!profile)
        return;

    m_profiles.append(profile);
    RefPtr<ScriptCallStack> callStack(createScriptCallStackForConsole(exec, 1));
    InspectorInstrumentation::addProfile(&m_page, profile, callStack);
}

void PageConsole::time(JSC::ExecState*, const String& title)
{
    InspectorInstrumentation::startConsoleTiming(&m_page.mainFrame(), title);
}

void PageConsole::timeEnd(JSC::ExecState* exec, const String& title)
{
    RefPtr<ScriptCallStack> callStack(createScriptCallStackForConsole(exec, 1));
    InspectorInstrumentation::stopConsoleTiming(&m_page.mainFrame(), title, callStack.release());
}

void PageConsole::timeStamp(JSC::ExecState*, PassRefPtr<ScriptArguments> arguments)
{
    InspectorInstrumentation::consoleTimeStamp(&m_page.mainFrame(), arguments);
}

void PageConsole::clearProfiles()
{
    m_profiles.clear();
}

} // namespace WebCore
