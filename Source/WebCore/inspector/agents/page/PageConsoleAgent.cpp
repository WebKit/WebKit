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
#include "LogInitialization.h"
#include "Logging.h"
#include "Node.h"
#include "Page.h"
#include "WebInjectedScriptManager.h"
#include <JavaScriptCore/ConsoleMessage.h>

namespace WebCore {

using namespace Inspector;

PageConsoleAgent::PageConsoleAgent(PageAgentContext& context)
    : WebConsoleAgent(context)
    , m_inspectedPage(context.inspectedPage)
{
}

PageConsoleAgent::~PageConsoleAgent() = default;

Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::Console::Channel>>> PageConsoleAgent::getLoggingChannels()
{
    auto channels = JSON::ArrayOf<Inspector::Protocol::Console::Channel>::create();

    auto addLogChannel = [&] (Inspector::Protocol::Console::ChannelSource source) {
        auto* logChannel = getLogChannel(Inspector::Protocol::Helpers::getEnumConstantValue(source));
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
            .setSource(source)
            .setLevel(level)
            .release();
        channels->addItem(WTFMove(channel));
    };
    addLogChannel(Inspector::Protocol::Console::ChannelSource::XML);
    addLogChannel(Inspector::Protocol::Console::ChannelSource::JavaScript);
    addLogChannel(Inspector::Protocol::Console::ChannelSource::Network);
    addLogChannel(Inspector::Protocol::Console::ChannelSource::ConsoleAPI);
    addLogChannel(Inspector::Protocol::Console::ChannelSource::Storage);
    addLogChannel(Inspector::Protocol::Console::ChannelSource::Appcache);
    addLogChannel(Inspector::Protocol::Console::ChannelSource::Rendering);
    addLogChannel(Inspector::Protocol::Console::ChannelSource::CSS);
    addLogChannel(Inspector::Protocol::Console::ChannelSource::Security);
    addLogChannel(Inspector::Protocol::Console::ChannelSource::ContentBlocker);
    addLogChannel(Inspector::Protocol::Console::ChannelSource::Media);
    addLogChannel(Inspector::Protocol::Console::ChannelSource::MediaSource);
    addLogChannel(Inspector::Protocol::Console::ChannelSource::WebRTC);
    addLogChannel(Inspector::Protocol::Console::ChannelSource::ITPDebug);
    addLogChannel(Inspector::Protocol::Console::ChannelSource::PrivateClickMeasurement);
    addLogChannel(Inspector::Protocol::Console::ChannelSource::PaymentRequest);
    addLogChannel(Inspector::Protocol::Console::ChannelSource::Other);

    return channels;
}

Inspector::Protocol::ErrorStringOr<void> PageConsoleAgent::setLoggingChannelLevel(Inspector::Protocol::Console::ChannelSource source, Inspector::Protocol::Console::ChannelLevel level)
{
    switch (level) {
    case Inspector::Protocol::Console::ChannelLevel::Off:
        m_inspectedPage.configureLoggingChannel(Inspector::Protocol::Helpers::getEnumConstantValue(source), WTFLogChannelState::Off, WTFLogLevel::Error);
        return { };

    case Inspector::Protocol::Console::ChannelLevel::Basic:
        m_inspectedPage.configureLoggingChannel(Inspector::Protocol::Helpers::getEnumConstantValue(source), WTFLogChannelState::On, WTFLogLevel::Info);
        return { };

    case Inspector::Protocol::Console::ChannelLevel::Verbose:
        m_inspectedPage.configureLoggingChannel(Inspector::Protocol::Helpers::getEnumConstantValue(source), WTFLogChannelState::On, WTFLogLevel::Debug);
        return { };
    }

    ASSERT_NOT_REACHED();
    return { };
}

} // namespace WebCore
