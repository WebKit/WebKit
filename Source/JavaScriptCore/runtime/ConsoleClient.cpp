/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ConsoleClient.h"

#include "CatchScope.h"
#include "JSCJSValueInlines.h"
#include "JSGlobalObject.h"
#include "ScriptArguments.h"
#include "ScriptCallStack.h"
#include "ScriptCallStackFactory.h"
#include <wtf/Assertions.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

using namespace Inspector;

namespace JSC {

static void appendURLAndPosition(StringBuilder& builder, const String& url, unsigned lineNumber, unsigned columnNumber)
{
    if (url.isEmpty())
        return;

    builder.append(url);

    if (lineNumber > 0) {
        builder.append(':');
        builder.appendNumber(lineNumber);
    }

    if (columnNumber > 0) {
        builder.append(':');
        builder.appendNumber(columnNumber);
    }
}

static void appendMessagePrefix(StringBuilder& builder, MessageSource source, MessageType type, MessageLevel level)
{
    String sourceString;
    switch (source) {
    case MessageSource::ConsoleAPI:
        // Default, no need to be more specific.
        break;
    case MessageSource::XML:
        sourceString = "XML"_s;
        break;
    case MessageSource::JS:
        sourceString = "JS"_s;
        break;
    case MessageSource::Network:
        sourceString = "NETWORK"_s;
        break;
    case MessageSource::Storage:
        sourceString = "STORAGE"_s;
        break;
    case MessageSource::AppCache:
        sourceString = "APPCACHE"_s;
        break;
    case MessageSource::Rendering:
        sourceString = "RENDERING"_s;
        break;
    case MessageSource::CSS:
        sourceString = "CSS"_s;
        break;
    case MessageSource::Security:
        sourceString = "SECURITY"_s;
        break;
    case MessageSource::ContentBlocker:
        sourceString = "CONTENTBLOCKER"_s;
        break;
    case MessageSource::Media:
        sourceString = "MEDIA"_s;
        break;
    case MessageSource::MediaSource:
        sourceString = "MEDIASOURCE"_s;
        break;
    case MessageSource::WebRTC:
        sourceString = "WEBRTC"_s;
        break;
    case MessageSource::ITPDebug:
        sourceString = "ITPDEBUG"_s;
        break;
    case MessageSource::AdClickAttribution:
        sourceString = "ADCLICKATTRIBUTION"_s;
        break;
    case MessageSource::Other:
        sourceString = "OTHER"_s;
        break;
    }

    String typeString;
    switch (type) {
    case MessageType::Log:
        // Default, no need to be more specific.
        break;
    case MessageType::Clear:
        typeString = "CLEAR"_s;
        break;
    case MessageType::Dir:
        typeString = "DIR"_s;
        break;
    case MessageType::DirXML:
        typeString = "DIRXML"_s;
        break;
    case MessageType::Table:
        typeString = "TABLE"_s;
        break;
    case MessageType::Trace:
        typeString = "TRACE"_s;
        break;
    case MessageType::StartGroup:
        typeString = "STARTGROUP"_s;
        break;
    case MessageType::StartGroupCollapsed:
        typeString = "STARTGROUPCOLLAPSED"_s;
        break;
    case MessageType::EndGroup:
        typeString = "ENDGROUP"_s;
        break;
    case MessageType::Assert:
        typeString = "ASSERT"_s;
        break;
    case MessageType::Timing:
        typeString = "TIMING"_s;
        break;
    case MessageType::Profile:
        typeString = "PROFILE"_s;
        break;
    case MessageType::ProfileEnd:
        typeString = "PROFILEEND"_s;
        break;
    case MessageType::Image:
        typeString = "IMAGE"_s;
        break;
    }

    String levelString;
    switch (level) {
    case MessageLevel::Log:
        // Default, no need to be more specific.
        if (type == MessageType::Log)
            levelString = "LOG"_s;
        break;
    case MessageLevel::Debug:
        levelString = "DEBUG"_s;
        break;
    case MessageLevel::Info:
        levelString = "INFO"_s;
        break;
    case MessageLevel::Warning:
        levelString = "WARN"_s;
        break;
    case MessageLevel::Error:
        levelString = "ERROR"_s;
        break;
    }

    builder.append("CONSOLE");
    if (!sourceString.isEmpty())
        builder.append(' ', sourceString);
    if (!typeString.isEmpty())
        builder.append(' ', typeString);
    if (!levelString.isEmpty())
        builder.append(' ', levelString);
}

void ConsoleClient::printConsoleMessage(MessageSource source, MessageType type, MessageLevel level, const String& message, const String& url, unsigned lineNumber, unsigned columnNumber)
{
    StringBuilder builder;

    if (!url.isEmpty()) {
        appendURLAndPosition(builder, url, lineNumber, columnNumber);
        builder.appendLiteral(": ");
    }

    appendMessagePrefix(builder, source, type, level);
    builder.append(' ');
    builder.append(message);

    WTFLogAlways("%s", builder.toString().utf8().data());
}

void ConsoleClient::printConsoleMessageWithArguments(MessageSource source, MessageType type, MessageLevel level, JSC::JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments)
{
    bool isTraceMessage = type == MessageType::Trace;
    size_t stackSize = isTraceMessage ? ScriptCallStack::maxCallStackSizeToCapture : 1;
    Ref<ScriptCallStack> callStack = createScriptCallStackForConsole(globalObject, stackSize);
    const ScriptCallFrame& lastCaller = callStack->at(0);

    StringBuilder builder;

    if (!lastCaller.sourceURL().isEmpty()) {
        appendURLAndPosition(builder, lastCaller.sourceURL(), lastCaller.lineNumber(), lastCaller.columnNumber());
        builder.appendLiteral(": ");
    }

    appendMessagePrefix(builder, source, type, level);
    for (size_t i = 0; i < arguments->argumentCount(); ++i) {
        builder.append(' ');
        auto* globalObject = arguments->globalObject();
        auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
        builder.append(arguments->argumentAt(i).toWTFString(globalObject));
        scope.clearException();
    }

    WTFLogAlways("%s", builder.toString().utf8().data());

    if (isTraceMessage) {
        for (size_t i = 0; i < callStack->size(); ++i) {
            const ScriptCallFrame& callFrame = callStack->at(i);
            String functionName = String(callFrame.functionName());
            if (functionName.isEmpty())
                functionName = "(unknown)"_s;

            StringBuilder callFrameBuilder;
            callFrameBuilder.appendNumber(i);
            callFrameBuilder.appendLiteral(": ");
            callFrameBuilder.append(functionName);
            callFrameBuilder.append('(');
            appendURLAndPosition(callFrameBuilder, callFrame.sourceURL(), callFrame.lineNumber(), callFrame.columnNumber());
            callFrameBuilder.append(')');

            WTFLogAlways("%s", callFrameBuilder.toString().utf8().data());
        }
    }
}

void ConsoleClient::internalMessageWithTypeAndLevel(MessageType type, MessageLevel level, JSC::JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments, ArgumentRequirement argumentRequirement)
{
    if (argumentRequirement == ArgumentRequired && !arguments->argumentCount())
        return;

    messageWithTypeAndLevel(type, level, globalObject, WTFMove(arguments));
}

void ConsoleClient::logWithLevel(JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments, MessageLevel level)
{
    internalMessageWithTypeAndLevel(MessageType::Log, level, globalObject, WTFMove(arguments), ArgumentRequired);
}

void ConsoleClient::clear(JSGlobalObject* globalObject)
{
    internalMessageWithTypeAndLevel(MessageType::Clear, MessageLevel::Log, globalObject, ScriptArguments::create(globalObject, { }), ArgumentNotRequired);
}

void ConsoleClient::dir(JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments)
{
    internalMessageWithTypeAndLevel(MessageType::Dir, MessageLevel::Log, globalObject, WTFMove(arguments), ArgumentRequired);
}

void ConsoleClient::dirXML(JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments)
{
    internalMessageWithTypeAndLevel(MessageType::DirXML, MessageLevel::Log, globalObject, WTFMove(arguments), ArgumentRequired);
}

void ConsoleClient::table(JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments)
{
    internalMessageWithTypeAndLevel(MessageType::Table, MessageLevel::Log, globalObject, WTFMove(arguments), ArgumentRequired);
}

void ConsoleClient::trace(JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments)
{
    internalMessageWithTypeAndLevel(MessageType::Trace, MessageLevel::Log, globalObject, WTFMove(arguments), ArgumentNotRequired);
}

void ConsoleClient::assertion(JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments)
{
    internalMessageWithTypeAndLevel(MessageType::Assert, MessageLevel::Error, globalObject, WTFMove(arguments), ArgumentNotRequired);
}

void ConsoleClient::group(JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments)
{
    internalMessageWithTypeAndLevel(MessageType::StartGroup, MessageLevel::Log, globalObject, WTFMove(arguments), ArgumentNotRequired);
}

void ConsoleClient::groupCollapsed(JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments)
{
    internalMessageWithTypeAndLevel(MessageType::StartGroupCollapsed, MessageLevel::Log, globalObject, WTFMove(arguments), ArgumentNotRequired);
}

void ConsoleClient::groupEnd(JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments)
{
    internalMessageWithTypeAndLevel(MessageType::EndGroup, MessageLevel::Log, globalObject, WTFMove(arguments), ArgumentNotRequired);
}

} // namespace JSC
