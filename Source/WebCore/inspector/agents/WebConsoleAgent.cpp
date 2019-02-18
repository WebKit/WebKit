/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "WebConsoleAgent.h"

#include "CommandLineAPIHost.h"
#include "DOMWindow.h"
#include "Logging.h"
#include "ResourceError.h"
#include "ResourceResponse.h"
#include "ScriptState.h"
#include "WebInjectedScriptManager.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/JSCInlines.h>
#include <wtf/text/StringBuilder.h>


namespace WebCore {

using namespace Inspector;

WebConsoleAgent::WebConsoleAgent(AgentContext& context, InspectorHeapAgent* heapAgent)
    : InspectorConsoleAgent(context, heapAgent)
{
}

void WebConsoleAgent::getLoggingChannels(ErrorString&, RefPtr<JSON::ArrayOf<Inspector::Protocol::Console::Channel>>& channels)
{
    static const struct ChannelTable {
        NeverDestroyed<String> name;
        Inspector::Protocol::Console::ChannelSource source;
    } channelTable[] = {
        { MAKE_STATIC_STRING_IMPL("WebRTC"), Inspector::Protocol::Console::ChannelSource::WebRTC },
        { MAKE_STATIC_STRING_IMPL("Media"), Inspector::Protocol::Console::ChannelSource::Media },
        { MAKE_STATIC_STRING_IMPL("MediaSource"), Inspector::Protocol::Console::ChannelSource::MediaSource },
    };

    channels = JSON::ArrayOf<Inspector::Protocol::Console::Channel>::create();

    size_t length = WTF_ARRAY_LENGTH(channelTable);
    for (size_t i = 0; i < length; ++i) {
        auto* logChannel = getLogChannel(channelTable[i].name);
        if (!logChannel)
            return;

        auto level = Inspector::Protocol::Console::ChannelLevel::Off;
        if (logChannel->state != WTFLogChannelOff) {
            switch (logChannel->level) {
            case WTFLogLevelAlways:
            case WTFLogLevelError:
            case WTFLogLevelWarning:
                level = Inspector::Protocol::Console::ChannelLevel::Basic;
                break;
            case WTFLogLevelInfo:
            case WTFLogLevelDebug:
                level = Inspector::Protocol::Console::ChannelLevel::Verbose;
                break;
            }
        }

        auto channel = Inspector::Protocol::Console::Channel::create()
            .setSource(channelTable[i].source)
            .setLevel(level)
            .release();
        channels->addItem(WTFMove(channel));
    }
}

static Optional<std::pair<WTFLogChannelState, WTFLogLevel>> channelConfigurationForString(const String& levelString)
{
    WTFLogChannelState state;
    WTFLogLevel level;

    if (equalIgnoringASCIICase(levelString, "off")) {
        state = WTFLogChannelOff;
        level = WTFLogLevelError;
    } else {
        state = WTFLogChannelOn;
        if (equalIgnoringASCIICase(levelString, "basic"))
            level = WTFLogLevelWarning;
        else if (equalIgnoringASCIICase(levelString, "verbose"))
            level = WTFLogLevelDebug;
        else
            return WTF::nullopt;
    }

    return { { state, level } };
}

void WebConsoleAgent::setLoggingChannelLevel(ErrorString& errorString, const String& channelName, const String& channelLevel)
{
    auto* channel = getLogChannel(channelName.utf8().data());
    if (!channel) {
        errorString = "Logging channel not found"_s;
        return;
    }

    auto configuration = channelConfigurationForString(channelLevel);
    if (!configuration) {
        errorString = "Invalid logging level"_s;
        return;
    }

    channel->state = configuration.value().first;
    channel->level = configuration.value().second;
}

void WebConsoleAgent::frameWindowDiscarded(DOMWindow* window)
{
    for (auto& message : m_consoleMessages) {
        JSC::ExecState* exec = message->scriptState();
        if (!exec)
            continue;
        if (domWindowFromExecState(exec) != window)
            continue;
        message->clear();
    }

    static_cast<WebInjectedScriptManager&>(m_injectedScriptManager).discardInjectedScriptsFor(window);
}

void WebConsoleAgent::didReceiveResponse(unsigned long requestIdentifier, const ResourceResponse& response)
{
    if (!m_injectedScriptManager.inspectorEnvironment().developerExtrasEnabled())
        return;

    if (response.httpStatusCode() >= 400) {
        String message = makeString("Failed to load resource: the server responded with a status of ", response.httpStatusCode(), " (", response.httpStatusText(), ')');
        addMessageToConsole(std::make_unique<ConsoleMessage>(MessageSource::Network, MessageType::Log, MessageLevel::Error, message, response.url().string(), 0, 0, nullptr, requestIdentifier));
    }
}

void WebConsoleAgent::didFailLoading(unsigned long requestIdentifier, const ResourceError& error)
{
    if (!m_injectedScriptManager.inspectorEnvironment().developerExtrasEnabled())
        return;

    // Report failures only.
    if (error.isCancellation())
        return;

    StringBuilder message;
    message.appendLiteral("Failed to load resource");
    if (!error.localizedDescription().isEmpty()) {
        message.appendLiteral(": ");
        message.append(error.localizedDescription());
    }

    addMessageToConsole(std::make_unique<ConsoleMessage>(MessageSource::Network, MessageType::Log, MessageLevel::Error, message.toString(), error.failingURL(), 0, 0, nullptr, requestIdentifier));
}

} // namespace WebCore
