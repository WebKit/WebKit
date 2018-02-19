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

#include "CanvasRenderingContext2D.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "Frame.h"
#include "HTMLCanvasElement.h"
#include "InspectorController.h"
#include "InspectorInstrumentation.h"
#include "JSCanvasRenderingContext2D.h"
#include "JSHTMLCanvasElement.h"
#include "JSMainThreadExecState.h"
#include "JSOffscreenCanvas.h"
#include "MainFrame.h"
#include "OffscreenCanvas.h"
#include "Page.h"
#include "ScriptableDocumentParser.h"
#include "Settings.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/ScriptArguments.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <JavaScriptCore/ScriptValue.h>
#include <wtf/text/WTFString.h>

#if ENABLE(WEBGL)
#include "JSWebGLRenderingContext.h"
#include "WebGLRenderingContext.h"
#endif


namespace WebCore {
using namespace Inspector;

PageConsoleClient::PageConsoleClient(Page& page)
    : m_page(page)
{
}

PageConsoleClient::~PageConsoleClient() = default;

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

static void getParserLocationForConsoleMessage(Document* document, String& url, unsigned& line, unsigned& column)
{
    if (!document)
        return;

    // We definitely cannot associate the message with a location being parsed if we are not even parsing.
    if (!document->parsing())
        return;

    ScriptableDocumentParser* parser = document->scriptableDocumentParser();
    if (!parser)
        return;

    // When the parser waits for scripts, any messages must be coming from some other source, and are not related to the location of the script element that made the parser wait.
    if (!parser->shouldAssociateConsoleMessagesWithTextPosition())
        return;

    url = document->url().string();
    TextPosition position = parser->textPosition();
    line = position.m_line.oneBasedInt();
    column = position.m_column.oneBasedInt();
}

void PageConsoleClient::addMessage(std::unique_ptr<Inspector::ConsoleMessage>&& consoleMessage)
{
    if (consoleMessage->source() != MessageSource::CSS && !m_page.usesEphemeralSession()) {
        m_page.chrome().client().addMessageToConsole(consoleMessage->source(), consoleMessage->level(), consoleMessage->message(), consoleMessage->line(), consoleMessage->column(), consoleMessage->url());

        if (m_page.settings().logsPageMessagesToSystemConsoleEnabled() || shouldPrintExceptions())
            ConsoleClient::printConsoleMessage(MessageSource::ConsoleAPI, MessageType::Log, consoleMessage->level(), consoleMessage->message(), consoleMessage->url(), consoleMessage->line(), consoleMessage->column());
    }

    InspectorInstrumentation::addMessageToConsole(m_page, WTFMove(consoleMessage));
}

void PageConsoleClient::addMessage(MessageSource source, MessageLevel level, const String& message, unsigned long requestIdentifier, Document* document)
{
    String url;
    unsigned line = 0;
    unsigned column = 0;
    getParserLocationForConsoleMessage(document, url, line, column);

    addMessage(source, level, message, url, line, column, 0, JSMainThreadExecState::currentState(), requestIdentifier);
}

void PageConsoleClient::addMessage(MessageSource source, MessageLevel level, const String& message, Ref<ScriptCallStack>&& callStack)
{
    addMessage(source, level, message, String(), 0, 0, WTFMove(callStack), 0);
}

void PageConsoleClient::addMessage(MessageSource source, MessageLevel level, const String& messageText, const String& suggestedURL, unsigned suggestedLineNumber, unsigned suggestedColumnNumber, RefPtr<ScriptCallStack>&& callStack, JSC::ExecState* state, unsigned long requestIdentifier)
{
    if (muteCount && source != MessageSource::ConsoleAPI)
        return;

    std::unique_ptr<Inspector::ConsoleMessage> message;

    if (callStack)
        message = std::make_unique<Inspector::ConsoleMessage>(source, MessageType::Log, level, messageText, callStack.releaseNonNull(), requestIdentifier);
    else
        message = std::make_unique<Inspector::ConsoleMessage>(source, MessageType::Log, level, messageText, suggestedURL, suggestedLineNumber, suggestedColumnNumber, state, requestIdentifier);

    addMessage(WTFMove(message));
}


void PageConsoleClient::messageWithTypeAndLevel(MessageType type, MessageLevel level, JSC::ExecState* exec, Ref<Inspector::ScriptArguments>&& arguments)
{
    String messageText;
    bool gotMessage = arguments->getFirstArgumentAsString(messageText);

    auto message = std::make_unique<Inspector::ConsoleMessage>(MessageSource::ConsoleAPI, type, level, messageText, arguments.copyRef(), exec);

    String url = message->url();
    unsigned lineNumber = message->line();
    unsigned columnNumber = message->column();

    InspectorInstrumentation::addMessageToConsole(m_page, WTFMove(message));

    if (m_page.usesEphemeralSession())
        return;

    if (gotMessage)
        m_page.chrome().client().addMessageToConsole(MessageSource::ConsoleAPI, level, messageText, lineNumber, columnNumber, url);

    if (m_page.settings().logsPageMessagesToSystemConsoleEnabled() || PageConsoleClient::shouldPrintExceptions())
        ConsoleClient::printConsoleMessageWithArguments(MessageSource::ConsoleAPI, type, level, exec, WTFMove(arguments));
}

void PageConsoleClient::count(JSC::ExecState* exec, Ref<ScriptArguments>&& arguments)
{
    InspectorInstrumentation::consoleCount(m_page, exec, WTFMove(arguments));
}

void PageConsoleClient::profile(JSC::ExecState* exec, const String& title)
{
    // FIXME: <https://webkit.org/b/153499> Web Inspector: console.profile should use the new Sampling Profiler
    InspectorInstrumentation::startProfiling(m_page, exec, title);
}

void PageConsoleClient::profileEnd(JSC::ExecState* exec, const String& title)
{
    // FIXME: <https://webkit.org/b/153499> Web Inspector: console.profile should use the new Sampling Profiler
    InspectorInstrumentation::stopProfiling(m_page, exec, title);
}

void PageConsoleClient::takeHeapSnapshot(JSC::ExecState*, const String& title)
{
    InspectorInstrumentation::takeHeapSnapshot(m_page.mainFrame(), title);
}

void PageConsoleClient::time(JSC::ExecState*, const String& title)
{
    InspectorInstrumentation::startConsoleTiming(m_page.mainFrame(), title);
}

void PageConsoleClient::timeEnd(JSC::ExecState* exec, const String& title)
{
    InspectorInstrumentation::stopConsoleTiming(m_page.mainFrame(), title, createScriptCallStackForConsole(exec, 1));
}

void PageConsoleClient::timeStamp(JSC::ExecState*, Ref<ScriptArguments>&& arguments)
{
    InspectorInstrumentation::consoleTimeStamp(m_page.mainFrame(), WTFMove(arguments));
}

void PageConsoleClient::record(JSC::ExecState* exec, Ref<ScriptArguments>&& arguments)
{
    if (arguments->argumentCount() < 1)
        return;

    JSC::JSObject* target = arguments->argumentAt(0).jsValue().getObject();
    if (!target)
        return;

    JSC::JSObject* options = nullptr;
    if (arguments->argumentCount() >= 2)
        options = arguments->argumentAt(1).jsValue().getObject();

    if (HTMLCanvasElement* canvasElement = JSHTMLCanvasElement::toWrapped(*target->vm(), target)) {
        if (CanvasRenderingContext* context = canvasElement->renderingContext())
            InspectorInstrumentation::consoleStartRecordingCanvas(*context, *exec, options);
    } else if (OffscreenCanvas* offscreenCanvas = JSOffscreenCanvas::toWrapped(*target->vm(), target)) {
        if (CanvasRenderingContext* context = offscreenCanvas->renderingContext())
            InspectorInstrumentation::consoleStartRecordingCanvas(*context, *exec, options);
    } else if (CanvasRenderingContext2D* context2d = JSCanvasRenderingContext2D::toWrapped(*target->vm(), target))
        InspectorInstrumentation::consoleStartRecordingCanvas(*context2d, *exec, options);
#if ENABLE(WEBGL)
    else if (WebGLRenderingContext* contextWebGL = JSWebGLRenderingContext::toWrapped(*target->vm(), target))
        InspectorInstrumentation::consoleStartRecordingCanvas(*contextWebGL, *exec, options);
#endif
}

void PageConsoleClient::recordEnd(JSC::ExecState*, Ref<ScriptArguments>&& arguments)
{
    if (arguments->argumentCount() < 1)
        return;

    JSC::JSObject* target = arguments->argumentAt(0).jsValue().getObject();
    if (!target)
        return;

    if (HTMLCanvasElement* canvasElement = JSHTMLCanvasElement::toWrapped(*target->vm(), target)) {
        if (CanvasRenderingContext* context = canvasElement->renderingContext())
            InspectorInstrumentation::didFinishRecordingCanvasFrame(*context, true);
    } else if (OffscreenCanvas* offscreenCanvas = JSOffscreenCanvas::toWrapped(*target->vm(), target)) {
        if (CanvasRenderingContext* context = offscreenCanvas->renderingContext())
            InspectorInstrumentation::didFinishRecordingCanvasFrame(*context, true);
    } else if (CanvasRenderingContext2D* context2d = JSCanvasRenderingContext2D::toWrapped(*target->vm(), target))
        InspectorInstrumentation::didFinishRecordingCanvasFrame(*context2d, true);
#if ENABLE(WEBGL)
    else if (WebGLRenderingContext* contextWebGL = JSWebGLRenderingContext::toWrapped(*target->vm(), target))
        InspectorInstrumentation::didFinishRecordingCanvasFrame(*contextWebGL, true);
#endif
}

} // namespace WebCore
