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

void PageConsoleAgent::clearMessages(ErrorString& errorString)
{
    if (auto* domAgent = m_instrumentingAgents.inspectorDOMAgent())
        domAgent->releaseDanglingNodes();

    WebConsoleAgent::clearMessages(errorString);
}

void PageConsoleAgent::getLoggingChannels(ErrorString&, RefPtr<JSON::ArrayOf<Inspector::Protocol::Console::Channel>>& channels)
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
        if (logChannel->state != WTFLogChannelState::Off) {
            switch (logChannel->level) {
            case WTFLogLevel::Always:
            case WTFLogLevel::Error:
            case WTFLogLevel::Warning:
            case WTFLogLevel::Info:
                level = Inspector::Protocol::Console::ChannelLevel::Basic;
                break;
            case WTFLogLevel::Debug:
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
    if (equalIgnoringASCIICase(levelString, "off"))
        return { { WTFLogChannelState::Off, WTFLogLevel::Error } };

    if (equalIgnoringASCIICase(levelString, "basic"))
        return { { WTFLogChannelState::On, WTFLogLevel::Info } };

    if (equalIgnoringASCIICase(levelString, "verbose"))
        return { { WTFLogChannelState::On, WTFLogLevel::Debug } };

    return WTF::nullopt;
}

void PageConsoleAgent::setLoggingChannelLevel(ErrorString& errorString, const String& channelName, const String& channelLevel)
{
    auto configuration = channelConfigurationForString(channelLevel);
    if (!configuration) {
        errorString = makeString("Unknown channelLevel: "_s, channelLevel);
        return;
    }

    m_inspectedPage.configureLoggingChannel(channelName, configuration.value().first, configuration.value().second);
}

} // namespace WebCore
