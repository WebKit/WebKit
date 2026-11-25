/*
 * Copyright (C) 2013-2024 Apple Inc. All rights reserved.
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

#include "JSCJSValueInlines.h"
#include "JSGlobalObject.h"
#include "ScriptArguments.h"
#include "ScriptCallStack.h"
#include "ScriptCallStackFactory.h"
#include "TopExceptionScope.h"
#include <wtf/Assertions.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

using namespace Inspector;

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ConsoleClient);

static void appendURLAndPosition(StringBuilder& builder, StringImpl* url, unsigned lineNumber, unsigned columnNumber)
{
    if (isEmpty(url))
        return;

    builder.append(url);

    if (lineNumber > 0)
        builder.append(':', lineNumber);

    if (columnNumber > 0)
        builder.append(':', columnNumber);
}

static void appendMessagePrefix(StringBuilder& builder, MessageSource source, MessageType type, MessageLevel level)
{
    RefPtr<StringImpl> sourceString;
    switch (source) {
    case MessageSource::ConsoleAPI:
        // Default, no need to be more specific.
        break;
    case MessageSource::XML:
        sourceString = "XML"_si;
        break;
    case MessageSource::JS:
        sourceString = "JS"_si;
        break;
    case MessageSource::Network:
        sourceString = "NETWORK"_si;
        break;
    case MessageSource::Storage:
        sourceString = "STORAGE"_si;
        break;
    case MessageSource::Rendering:
        sourceString = "RENDERING"_si;
        break;
    case MessageSource::CSS:
        sourceString = "CSS"_si;
        break;
    case MessageSource::Accessibility:
        sourceString = "Accessibility"_si;
        break;
    case MessageSource::Security:
        sourceString = "SECURITY"_si;
        break;
    case MessageSource::ContentBlocker:
        sourceString = "CONTENTBLOCKER"_si;
        break;
    case MessageSource::Media:
        sourceString = "MEDIA"_si;
        break;
    case MessageSource::MediaSource:
        sourceString = "MEDIASOURCE"_si;
        break;
    case MessageSource::WebRTC:
        sourceString = "WEBRTC"_si;
        break;
    case MessageSource::ITPDebug:
        sourceString = "ITPDEBUG"_si;
        break;
    case MessageSource::PrivateClickMeasurement:
        sourceString = "PRIVATECLICKMEASUREMENT"_si;
        break;
    case MessageSource::PaymentRequest:
        sourceString = "PAYMENTREQUEST"_si;
        break;
    case MessageSource::Other:
        sourceString = "OTHER"_si;
        break;
    }

    RefPtr<StringImpl> typeString;
    switch (type) {
    case MessageType::Log:
        // Default, no need to be more specific.
        break;
    case MessageType::Clear:
        typeString = "CLEAR"_si;
        break;
    case MessageType::Dir:
        typeString = "DIR"_si;
        break;
    case MessageType::DirXML:
        typeString = "DIRXML"_si;
        break;
    case MessageType::Table:
        typeString = "TABLE"_si;
        break;
    case MessageType::Trace:
        typeString = "TRACE"_si;
        break;
    case MessageType::StartGroup:
        typeString = "STARTGROUP"_si;
        break;
    case MessageType::StartGroupCollapsed:
        typeString = "STARTGROUPCOLLAPSED"_si;
        break;
    case MessageType::EndGroup:
        typeString = "ENDGROUP"_si;
        break;
    case MessageType::Assert:
        typeString = "ASSERT"_si;
        break;
    case MessageType::Timing:
        typeString = "TIMING"_si;
        break;
    case MessageType::Profile:
        typeString = "PROFILE"_si;
        break;
    case MessageType::ProfileEnd:
        typeString = "PROFILEEND"_si;
        break;
    case MessageType::Image:
        typeString = "IMAGE"_si;
        break;
    }

    RefPtr<StringImpl> levelString;
    switch (level) {
    case MessageLevel::Log:
        // Default, no need to be more specific.
        if (type == MessageType::Log)
            levelString = "LOG"_si;
        break;
    case MessageLevel::Debug:
        levelString = "DEBUG"_si;
        break;
    case MessageLevel::Info:
        levelString = "INFO"_si;
        break;
    case MessageLevel::Warning:
        levelString = "WARN"_si;
        break;
    case MessageLevel::Error:
        levelString = "ERROR"_si;
        break;
    }

    builder.append("CONSOLE"_s);
    if (!isEmpty(sourceString))
        builder.append(' ', sourceString);
    if (!isEmpty(typeString))
        builder.append(' ', typeString);
    if (!isEmpty(levelString))
        builder.append(' ', levelString);
}

void ConsoleClient::printConsoleMessage(MessageSource source, MessageType type, MessageLevel level, StringImpl* message, StringImpl* url, unsigned lineNumber, unsigned columnNumber)
{
    StringBuilder builder;

    if (!isEmpty(url)) {
        appendURLAndPosition(builder, url, lineNumber, columnNumber);
        builder.append(": "_s);
    }

    appendMessagePrefix(builder, source, type, level);
    builder.append(' ', message);

    SAFE_WTFLOGALWAYS("%s", utf8(builder.toString()));
}

void ConsoleClient::printConsoleMessageWithArguments(MessageSource source, MessageType type, MessageLevel level, JSC::JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments)
{
    bool isTraceMessage = type == MessageType::Trace;
    size_t stackSize = isTraceMessage ? ScriptCallStack::maxCallStackSizeToCapture : 1;
    Ref<ScriptCallStack> callStack = createScriptCallStackForConsole(globalObject, stackSize);
    const ScriptCallFrame& lastCaller = callStack->at(0);

    StringBuilder builder(OverflowPolicy::RecordOverflow);

    if (!isEmpty(lastCaller.sourceURL())) {
        appendURLAndPosition(builder, lastCaller.sourceURL(), lastCaller.lineNumber(), lastCaller.columnNumber());
        builder.append(": "_s);
    }

    appendMessagePrefix(builder, source, type, level);
    for (size_t i = 0; i < arguments->argumentCount(); ++i) {
        builder.append(' ');
        auto* globalObject = arguments->globalObject();
        auto scope = DECLARE_TOP_EXCEPTION_SCOPE(globalObject->vm());
        builder.append(arguments->argumentAt(i).toWTFString(globalObject));
        scope.clearException();
    }

    if (builder.hasOverflowed())
        WTFLogAlways("Console message exceeded maximum length.");
    else
        SAFE_WTFLOGALWAYS("%s", utf8(builder.toString()));

    if (isTraceMessage) {
        for (size_t i = 0; i < callStack->size(); ++i) {
            const ScriptCallFrame& callFrame = callStack->at(i);
            RefPtr<StringImpl> functionName = callFrame.functionName();
            if (isEmpty(functionName))
                functionName = "(unknown)"_si;

            StringBuilder callFrameBuilder;
            callFrameBuilder.append(i, ": "_s, functionName, '(');
            appendURLAndPosition(callFrameBuilder, callFrame.sourceURL(), callFrame.lineNumber(), callFrame.columnNumber());
            callFrameBuilder.append(')');

            SAFE_WTFLOGALWAYS("%s", utf8(callFrameBuilder.toString()));
        }
    }
}

void ConsoleClient::internalMessageWithTypeAndLevel(MessageType type, MessageLevel level, JSC::JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments, ArgumentRequirement argumentRequirement)
{
    if (argumentRequirement == ArgumentRequirement::Yes && !arguments->argumentCount())
        return;

    messageWithTypeAndLevel(type, level, globalObject, WTF::move(arguments));
}

void ConsoleClient::logWithLevel(JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments, MessageLevel level)
{
    internalMessageWithTypeAndLevel(MessageType::Log, level, globalObject, WTF::move(arguments), ArgumentRequirement::Yes);
}

void ConsoleClient::clear(JSGlobalObject* globalObject)
{
    internalMessageWithTypeAndLevel(MessageType::Clear, MessageLevel::Log, globalObject, ScriptArguments::create(globalObject, { }), ArgumentRequirement::No);
}

void ConsoleClient::dir(JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments)
{
    internalMessageWithTypeAndLevel(MessageType::Dir, MessageLevel::Log, globalObject, WTF::move(arguments), ArgumentRequirement::Yes);
}

void ConsoleClient::dirXML(JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments)
{
    internalMessageWithTypeAndLevel(MessageType::DirXML, MessageLevel::Log, globalObject, WTF::move(arguments), ArgumentRequirement::Yes);
}

void ConsoleClient::table(JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments)
{
    internalMessageWithTypeAndLevel(MessageType::Table, MessageLevel::Log, globalObject, WTF::move(arguments), ArgumentRequirement::Yes);
}

void ConsoleClient::trace(JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments)
{
    internalMessageWithTypeAndLevel(MessageType::Trace, MessageLevel::Log, globalObject, WTF::move(arguments), ArgumentRequirement::No);
}

void ConsoleClient::assertion(JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments)
{
    internalMessageWithTypeAndLevel(MessageType::Assert, MessageLevel::Error, globalObject, WTF::move(arguments), ArgumentRequirement::No);
}

void ConsoleClient::group(JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments)
{
    internalMessageWithTypeAndLevel(MessageType::StartGroup, MessageLevel::Log, globalObject, WTF::move(arguments), ArgumentRequirement::No);
}

void ConsoleClient::groupCollapsed(JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments)
{
    internalMessageWithTypeAndLevel(MessageType::StartGroupCollapsed, MessageLevel::Log, globalObject, WTF::move(arguments), ArgumentRequirement::No);
}

void ConsoleClient::groupEnd(JSGlobalObject* globalObject, Ref<ScriptArguments>&& arguments)
{
    internalMessageWithTypeAndLevel(MessageType::EndGroup, MessageLevel::Log, globalObject, WTF::move(arguments), ArgumentRequirement::No);
}

} // namespace JSC
