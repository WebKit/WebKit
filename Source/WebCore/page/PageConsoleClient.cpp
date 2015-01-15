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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#include "PageConsoleClient.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "Frame.h"
#include "InspectorConsoleInstrumentation.h"
#include "InspectorController.h"
#include "JSMainThreadExecState.h"
#include "MainFrame.h"
#include "Page.h"
#include "ScriptableDocumentParser.h"
#include "Settings.h"
#include <bindings/ScriptValue.h>
#include <inspector/ConsoleMessage.h>
#include <inspector/ScriptArguments.h>
#include <inspector/ScriptCallStack.h>
#include <inspector/ScriptCallStackFactory.h>

using namespace Inspector;

namespace WebCore {

PageConsoleClient::PageConsoleClient(Page& page)
    : m_page(page)
{
}

PageConsoleClient::~PageConsoleClient()
{
}

static int muteCount = 0;
static bool printExceptions = false;

bool PageConsoleClient::shouldPrintExceptions()
{
    return printExceptions;
}

void PageConsoleClient::setShouldPrintExceptions(bool print)
{
    printExceptions = print;
}

void PageConsoleClient::mute()
{
    muteCount++;
}

void PageConsoleClient::unmute()
{
    ASSERT(muteCount > 0);
    muteCount--;
}

void PageConsoleClient::addMessage(MessageSource source, MessageLevel level, const String& message, unsigned long requestIdentifier, Document* document)
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

void PageConsoleClient::addMessage(MessageSource source, MessageLevel level, const String& message, RefPtr<ScriptCallStack>&& callStack)
{
    addMessage(source, level, message, String(), 0, 0, WTF::move(callStack), 0);
}

void PageConsoleClient::addMessage(MessageSource source, MessageLevel level, const String& messageText, const String& suggestedURL, unsigned suggestedLineNumber, unsigned suggestedColumnNumber, RefPtr<ScriptCallStack>&& callStack, JSC::ExecState* state, unsigned long requestIdentifier)
{
    if (muteCount && source != MessageSource::ConsoleAPI)
        return;

    std::unique_ptr<Inspector::ConsoleMessage> message;

    if (callStack)
        message = std::make_unique<Inspector::ConsoleMessage>(source, MessageType::Log, level, messageText, WTF::move(callStack), requestIdentifier);
    else
        message = std::make_unique<Inspector::ConsoleMessage>(source, MessageType::Log, level, messageText, suggestedURL, suggestedLineNumber, suggestedColumnNumber, state, requestIdentifier);

    String url = message->url();
    unsigned lineNumber = message->line();
    unsigned columnNumber = message->column();

    InspectorInstrumentation::addMessageToConsole(m_page, WTF::move(message));

    if (source == MessageSource::CSS)
        return;

    if (m_page.usesEphemeralSession())
        return;

    m_page.chrome().client().addMessageToConsole(source, level, messageText, lineNumber, columnNumber, url);

    if (!m_page.settings().logsPageMessagesToSystemConsoleEnabled() && !shouldPrintExceptions())
        return;

    ConsoleClient::printConsoleMessage(MessageSource::ConsoleAPI, MessageType::Log, level, messageText, url, lineNumber, columnNumber);
}


void PageConsoleClient::messageWithTypeAndLevel(MessageType type, MessageLevel level, JSC::ExecState* exec, RefPtr<Inspector::ScriptArguments>&& arguments)
{
    String messageText;
    bool gotMessage = arguments->getFirstArgumentAsString(messageText);

    auto message = std::make_unique<Inspector::ConsoleMessage>(MessageSource::ConsoleAPI, type, level, messageText, WTF::move(arguments), exec);

    String url = message->url();
    unsigned lineNumber = message->line();
    unsigned columnNumber = message->column();

    InspectorInstrumentation::addMessageToConsole(m_page, WTF::move(message));

    if (m_page.usesEphemeralSession())
        return;

    if (gotMessage)
        m_page.chrome().client().addMessageToConsole(MessageSource::ConsoleAPI, type, level, messageText, lineNumber, columnNumber, url);

    // FIXME: This doesn't work, we already moved out of arguments variable (regressed in <http://trac.webkit.org/changeset/178060>).
    if (m_page.settings().logsPageMessagesToSystemConsoleEnabled() || PageConsoleClient::shouldPrintExceptions())
        ConsoleClient::printConsoleMessageWithArguments(MessageSource::ConsoleAPI, type, level, exec, WTF::move(arguments));
}

void PageConsoleClient::count(JSC::ExecState* exec, RefPtr<ScriptArguments>&& arguments)
{
    InspectorInstrumentation::consoleCount(m_page, exec, WTF::move(arguments));
}

void PageConsoleClient::profile(JSC::ExecState* exec, const String& title)
{
    InspectorInstrumentation::startProfiling(m_page, exec, title);
}

void PageConsoleClient::profileEnd(JSC::ExecState* exec, const String& title)
{
    if (RefPtr<JSC::Profile> profile = InspectorInstrumentation::stopProfiling(m_page, exec, title))
        m_profiles.append(WTF::move(profile));
}

void PageConsoleClient::time(JSC::ExecState*, const String& title)
{
    InspectorInstrumentation::startConsoleTiming(m_page.mainFrame(), title);
}

void PageConsoleClient::timeEnd(JSC::ExecState* exec, const String& title)
{
    RefPtr<ScriptCallStack> callStack(createScriptCallStackForConsole(exec, 1));
    InspectorInstrumentation::stopConsoleTiming(m_page.mainFrame(), title, WTF::move(callStack));
}

void PageConsoleClient::timeStamp(JSC::ExecState*, RefPtr<ScriptArguments>&& arguments)
{
    InspectorInstrumentation::consoleTimeStamp(m_page.mainFrame(), WTF::move(arguments));
}

void PageConsoleClient::clearProfiles()
{
    m_profiles.clear();
}

} // namespace WebCore
