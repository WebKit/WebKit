/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PageConsoleAgent.h"

#include "CommandLineAPIHost.h"
#include "InspectorDOMAgent.h"
#include "InstrumentingAgents.h"
#include "Logging.h"
#include "Node.h"
#include "Page.h"
#include "WebInjectedScriptManager.h"
#include <JavaScriptCore/ConsoleMessage.h>

namespace WebCore {

using namespace Inspector;

PageConsoleAgent::PageConsoleAgent(PageAgentContext& context)
    : WebConsoleAgent(context)
    , m_instrumentingAgents(context.instrumentingAgents)
    , m_inspectedPage(context.inspectedPage)
{
}

PageConsoleAgent::~PageConsoleAgent() = default;

Protocol::ErrorStringOr<void> PageConsoleAgent::clearMessages()
{
    if (auto* domAgent = m_instrumentingAgents.persistentDOMAgent())
        domAgent->releaseDanglingNodes();

    return WebConsoleAgent::clearMessages();
}

Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Protocol::Console::Channel>>> PageConsoleAgent::getLoggingChannels()
{
    auto channels = JSON::ArrayOf<Protocol::Console::Channel>::create();

    auto addLogChannel = [&] (Protocol::Console::ChannelSource source) {
        auto* logChannel = getLogChannel(Protocol::Helpers::getEnumConstantValue(source));
        if (!logChannel)
            return;

        auto level = Protocol::Console::ChannelLevel::Off;
        if (logChannel->state != WTFLogChannelState::Off) {
            switch (logChannel->level) {
            case WTFLogLevel::Always:
            case WTFLogLevel::Error:
            case WTFLogLevel::Warning:
            case WTFLogLevel::Info:
                level = Protocol::Console::ChannelLevel::Basic;
                break;

            case WTFLogLevel::Debug:
                level = Protocol::Console::ChannelLevel::Verbose;
                break;
            }
        }

        auto channel = Protocol::Console::Channel::create()
            .setSource(source)
            .setLevel(level)
            .release();
        channels->addItem(WTFMove(channel));
    };
    addLogChannel(Protocol::Console::ChannelSource::XML);
    addLogChannel(Protocol::Console::ChannelSource::JavaScript);
    addLogChannel(Protocol::Console::ChannelSource::Network);
    addLogChannel(Protocol::Console::ChannelSource::ConsoleAPI);
    addLogChannel(Protocol::Console::ChannelSource::Storage);
    addLogChannel(Protocol::Console::ChannelSource::Appcache);
    addLogChannel(Protocol::Console::ChannelSource::Rendering);
    addLogChannel(Protocol::Console::ChannelSource::CSS);
    addLogChannel(Protocol::Console::ChannelSource::Security);
    addLogChannel(Protocol::Console::ChannelSource::ContentBlocker);
    addLogChannel(Protocol::Console::ChannelSource::Media);
    addLogChannel(Protocol::Console::ChannelSource::MediaSource);
    addLogChannel(Protocol::Console::ChannelSource::WebRTC);
    addLogChannel(Protocol::Console::ChannelSource::ITPDebug);
    addLogChannel(Protocol::Console::ChannelSource::AdClickAttribution);
    addLogChannel(Protocol::Console::ChannelSource::Other);

    return WTFMove(channels);
}

Protocol::ErrorStringOr<void> PageConsoleAgent::setLoggingChannelLevel(Protocol::Console::ChannelSource source, Protocol::Console::ChannelLevel level)
{
    switch (level) {
    case Protocol::Console::ChannelLevel::Off:
        m_inspectedPage.configureLoggingChannel(Protocol::Helpers::getEnumConstantValue(source), WTFLogChannelState::Off, WTFLogLevel::Error);
        return { };

    case Protocol::Console::ChannelLevel::Basic:
        m_inspectedPage.configureLoggingChannel(Protocol::Helpers::getEnumConstantValue(source), WTFLogChannelState::On, WTFLogLevel::Info);
        return { };

    case Protocol::Console::ChannelLevel::Verbose:
        m_inspectedPage.configureLoggingChannel(Protocol::Helpers::getEnumConstantValue(source), WTFLogChannelState::On, WTFLogLevel::Debug);
        return { };
    }

    ASSERT_NOT_REACHED();
    return { };
}

} // namespace WebCore
