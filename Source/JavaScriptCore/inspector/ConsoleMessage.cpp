/*
 * Copyright (C) 2007, 2008, 2014, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
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
#include "ConsoleMessage.h"

#include "IdentifiersFactory.h"
#include "InjectedScript.h"
#include "InjectedScriptManager.h"
#include "InspectorFrontendDispatchers.h"
#include "ScriptArguments.h"
#include "ScriptCallFrame.h"
#include "ScriptCallStack.h"
#include "ScriptCallStackFactory.h"

namespace Inspector {

ConsoleMessage::ConsoleMessage(MessageSource source, MessageType type, MessageLevel level, const String& message, unsigned long requestIdentifier)
    : m_source(source)
    , m_type(type)
    , m_level(level)
    , m_message(message)
    , m_url()
    , m_requestId(IdentifiersFactory::requestId(requestIdentifier))
{
}

ConsoleMessage::ConsoleMessage(MessageSource source, MessageType type, MessageLevel level, const String& message, const String& url, unsigned line, unsigned column, JSC::ExecState* state, unsigned long requestIdentifier)
    : m_source(source)
    , m_type(type)
    , m_level(level)
    , m_message(message)
    , m_url(url)
    , m_line(line)
    , m_column(column)
    , m_requestId(IdentifiersFactory::requestId(requestIdentifier))
{
    autogenerateMetadata(state);
}

ConsoleMessage::ConsoleMessage(MessageSource source, MessageType type, MessageLevel level, const String& message, Ref<ScriptCallStack>&& callStack, unsigned long requestIdentifier)
    : m_source(source)
    , m_type(type)
    , m_level(level)
    , m_message(message)
    , m_callStack(WTFMove(callStack))
    , m_url()
    , m_requestId(IdentifiersFactory::requestId(requestIdentifier))
{
    const ScriptCallFrame* frame = m_callStack ? m_callStack->firstNonNativeCallFrame() : nullptr;
    if (frame) {
        m_url = frame->sourceURL();
        m_line = frame->lineNumber();
        m_column = frame->columnNumber();
    }
}

ConsoleMessage::ConsoleMessage(MessageSource source, MessageType type, MessageLevel level, const String& message, Ref<ScriptArguments>&& arguments, JSC::ExecState* state, unsigned long requestIdentifier)
    : m_source(source)
    , m_type(type)
    , m_level(level)
    , m_message(message)
    , m_arguments(WTFMove(arguments))
    , m_url()
    , m_requestId(IdentifiersFactory::requestId(requestIdentifier))
{
    autogenerateMetadata(state);
}

ConsoleMessage::ConsoleMessage(MessageSource source, MessageType type, MessageLevel level, Vector<JSONLogValue>&& messages, JSC::ExecState* state, unsigned long requestIdentifier)
    : m_source(source)
    , m_type(type)
    , m_level(level)
    , m_url()
    , m_scriptState(state)
    , m_requestId(IdentifiersFactory::requestId(requestIdentifier))
{
    if (!messages.size())
        return;

    m_jsonLogValues.reserveInitialCapacity(messages.size());

    StringBuilder builder;
    for (auto& message : messages) {
        switch (message.type) {
        case JSONLogValue::Type::String:
            builder.append(message.value);
            break;
        case JSONLogValue::Type::JSON:
            if (builder.length()) {
                m_jsonLogValues.append({ JSONLogValue::Type::String, JSON::Value::create(builder.toString())->toJSONString() });
                builder.resize(0);
            }

            m_jsonLogValues.append(message);
            break;
        }
    }

    if (builder.length())
        m_jsonLogValues.append({ JSONLogValue::Type::String, JSON::Value::create(builder.toString())->toJSONString() });

    if (m_jsonLogValues.size())
        m_message = m_jsonLogValues[0].value;
}

ConsoleMessage::~ConsoleMessage()
{
}

void ConsoleMessage::autogenerateMetadata(JSC::ExecState* state)
{
    if (!state)
        return;

    if (m_type == MessageType::EndGroup)
        return;

    // FIXME: Should this really be using "for console" in the generic ConsoleMessage autogeneration? This can skip the first frame.
    m_callStack = createScriptCallStackForConsole(state);

    if (const ScriptCallFrame* frame = m_callStack->firstNonNativeCallFrame()) {
        m_url = frame->sourceURL();
        m_line = frame->lineNumber();
        m_column = frame->columnNumber();
        return;
    }
}

static Protocol::Console::ChannelSource messageSourceValue(MessageSource source)
{
    switch (source) {
    case MessageSource::XML: return Protocol::Console::ChannelSource::XML;
    case MessageSource::JS: return Protocol::Console::ChannelSource::JavaScript;
    case MessageSource::Network: return Protocol::Console::ChannelSource::Network;
    case MessageSource::ConsoleAPI: return Protocol::Console::ChannelSource::ConsoleAPI;
    case MessageSource::Storage: return Protocol::Console::ChannelSource::Storage;
    case MessageSource::AppCache: return Protocol::Console::ChannelSource::Appcache;
    case MessageSource::Rendering: return Protocol::Console::ChannelSource::Rendering;
    case MessageSource::CSS: return Protocol::Console::ChannelSource::CSS;
    case MessageSource::Security: return Protocol::Console::ChannelSource::Security;
    case MessageSource::ContentBlocker: return Protocol::Console::ChannelSource::ContentBlocker;
    case MessageSource::Other: return Protocol::Console::ChannelSource::Other;
    case MessageSource::Media: return Protocol::Console::ChannelSource::Media;
    case MessageSource::WebRTC: return Protocol::Console::ChannelSource::WebRTC;
    }
    return Protocol::Console::ChannelSource::Other;
}

static Protocol::Console::ConsoleMessage::Type messageTypeValue(MessageType type)
{
    switch (type) {
    case MessageType::Log: return Protocol::Console::ConsoleMessage::Type::Log;
    case MessageType::Clear: return Protocol::Console::ConsoleMessage::Type::Clear;
    case MessageType::Dir: return Protocol::Console::ConsoleMessage::Type::Dir;
    case MessageType::DirXML: return Protocol::Console::ConsoleMessage::Type::DirXML;
    case MessageType::Table: return Protocol::Console::ConsoleMessage::Type::Table;
    case MessageType::Trace: return Protocol::Console::ConsoleMessage::Type::Trace;
    case MessageType::StartGroup: return Protocol::Console::ConsoleMessage::Type::StartGroup;
    case MessageType::StartGroupCollapsed: return Protocol::Console::ConsoleMessage::Type::StartGroupCollapsed;
    case MessageType::EndGroup: return Protocol::Console::ConsoleMessage::Type::EndGroup;
    case MessageType::Assert: return Protocol::Console::ConsoleMessage::Type::Assert;
    case MessageType::Timing: return Protocol::Console::ConsoleMessage::Type::Timing;
    case MessageType::Profile: return Protocol::Console::ConsoleMessage::Type::Profile;
    case MessageType::ProfileEnd: return Protocol::Console::ConsoleMessage::Type::ProfileEnd;
    }
    return Protocol::Console::ConsoleMessage::Type::Log;
}

static Protocol::Console::ConsoleMessage::Level messageLevelValue(MessageLevel level)
{
    switch (level) {
    case MessageLevel::Log: return Protocol::Console::ConsoleMessage::Level::Log;
    case MessageLevel::Info: return Protocol::Console::ConsoleMessage::Level::Info;
    case MessageLevel::Warning: return Protocol::Console::ConsoleMessage::Level::Warning;
    case MessageLevel::Error: return Protocol::Console::ConsoleMessage::Level::Error;
    case MessageLevel::Debug: return Protocol::Console::ConsoleMessage::Level::Debug;
    }
    return Protocol::Console::ConsoleMessage::Level::Log;
}

void ConsoleMessage::addToFrontend(ConsoleFrontendDispatcher& consoleFrontendDispatcher, InjectedScriptManager& injectedScriptManager, bool generatePreview)
{
    auto messageObject = Protocol::Console::ConsoleMessage::create()
        .setSource(messageSourceValue(m_source))
        .setLevel(messageLevelValue(m_level))
        .setText(m_message)
        .release();

    // FIXME: only send out type for ConsoleAPI source messages.
    messageObject->setType(messageTypeValue(m_type));
    messageObject->setLine(static_cast<int>(m_line));
    messageObject->setColumn(static_cast<int>(m_column));
    messageObject->setUrl(m_url);
    messageObject->setRepeatCount(static_cast<int>(m_repeatCount));

    if (m_source == MessageSource::Network && !m_requestId.isEmpty())
        messageObject->setNetworkRequestId(m_requestId);

    if ((m_arguments && m_arguments->argumentCount()) || m_jsonLogValues.size()) {
        InjectedScript injectedScript = injectedScriptManager.injectedScriptFor(scriptState());
        if (!injectedScript.hasNoValue()) {
            auto argumentsObject = JSON::ArrayOf<Protocol::Runtime::RemoteObject>::create();
            if (m_arguments && m_arguments->argumentCount()) {
                if (m_type == MessageType::Table && generatePreview && m_arguments->argumentCount()) {
                    auto table = m_arguments->argumentAt(0);
                    auto columns = m_arguments->argumentCount() > 1 ? m_arguments->argumentAt(1) : JSC::JSValue();
                    auto inspectorValue = injectedScript.wrapTable(table, columns);
                    if (!inspectorValue) {
                        ASSERT_NOT_REACHED();
                        return;
                    }
                    argumentsObject->addItem(WTFMove(inspectorValue));
                    if (m_arguments->argumentCount() > 1)
                        argumentsObject->addItem(injectedScript.wrapObject(columns, "console"_s, true));
                } else {
                    for (unsigned i = 0; i < m_arguments->argumentCount(); ++i) {
                        auto inspectorValue = injectedScript.wrapObject(m_arguments->argumentAt(i), "console"_s, generatePreview);
                        if (!inspectorValue) {
                            ASSERT_NOT_REACHED();
                            return;
                        }
                        argumentsObject->addItem(WTFMove(inspectorValue));
                    }
                }
            }

            if (m_jsonLogValues.size()) {
                for (auto& message : m_jsonLogValues) {
                    if (message.value.isEmpty())
                        continue;
                    auto inspectorValue = injectedScript.wrapJSONString(message.value, "console"_s, generatePreview);
                    if (!inspectorValue)
                        continue;

                    argumentsObject->addItem(WTFMove(inspectorValue));
                }
            }

            if (argumentsObject->length())
                messageObject->setParameters(WTFMove(argumentsObject));
        }
    }

    if (m_callStack)
        messageObject->setStackTrace(m_callStack->buildInspectorArray());

    consoleFrontendDispatcher.messageAdded(WTFMove(messageObject));
}

void ConsoleMessage::updateRepeatCountInConsole(ConsoleFrontendDispatcher& consoleFrontendDispatcher)
{
    consoleFrontendDispatcher.messageRepeatCountUpdated(m_repeatCount);
}

bool ConsoleMessage::isEqual(ConsoleMessage* msg) const
{
    if (m_arguments) {
        if (!msg->m_arguments || !m_arguments->isEqual(*msg->m_arguments))
            return false;

        // Never treat objects as equal - their properties might change over time.
        for (size_t i = 0; i < m_arguments->argumentCount(); ++i) {
            if (m_arguments->argumentAt(i).isObject())
                return false;
        }
    } else if (msg->m_arguments)
        return false;

    if (m_callStack) {
        if (!m_callStack->isEqual(msg->m_callStack.get()))
            return false;
    } else if (msg->m_callStack)
        return false;

    if (m_jsonLogValues.size() || msg->m_jsonLogValues.size())
        return false;

    return msg->m_source == m_source
        && msg->m_type == m_type
        && msg->m_level == m_level
        && msg->m_message == m_message
        && msg->m_line == m_line
        && msg->m_column == m_column
        && msg->m_url == m_url
        && msg->m_requestId == m_requestId;
}

void ConsoleMessage::clear()
{
    if (!m_message)
        m_message = "<message collected>"_s;

    if (m_arguments)
        m_arguments = nullptr;
}

JSC::ExecState* ConsoleMessage::scriptState() const
{
    if (m_arguments)
        return m_arguments->globalState();

    if (m_scriptState)
        return m_scriptState;

    return nullptr;
}

unsigned ConsoleMessage::argumentCount() const
{
    if (m_arguments)
        return m_arguments->argumentCount();

    return 0;
}

} // namespace Inspector
