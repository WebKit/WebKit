/*
 * Copyright (C) 2021 Apple Inc.  All rights reserved.
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
#include "LogChannels.h"

#include <wtf/LoggingAccumulator.h>

namespace WTF {

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED

bool LogChannels::isLogChannelEnabled(const String& name)
{
    WTFLogChannel* channel = getLogChannel(name);
    if (!channel)
        return false;
    return channel->state != WTFLogChannelState::Off;
}

void LogChannels::setLogChannelToAccumulate(const String& name)
{
    WTFLogChannel* channel = getLogChannel(name);
    if (!channel)
        return;

    channel->state = WTFLogChannelState::OnWithAccumulation;
    m_logChannelsNeedInitialization = true;
}

void LogChannels::clearAllLogChannelsToAccumulate()
{
    resetAccumulatedLogs();
    for (auto* channel : m_logChannels) {
        if (channel->state == WTFLogChannelState::OnWithAccumulation)
            channel->state = WTFLogChannelState::Off;
    }

    m_logChannelsNeedInitialization = true;
}

void LogChannels::initializeLogChannelsIfNecessary(std::optional<String> logChannelString)
{
    if (!m_logChannelsNeedInitialization && !logChannelString)
        return;

    m_logChannelsNeedInitialization = false;

    String enabledChannelsString = logChannelString ? logChannelString.value() : logLevelString();
    WTFInitializeLogChannelStatesFromString(m_logChannels.data(), m_logChannels.size(), enabledChannelsString.utf8().data());
}

WTFLogChannel* LogChannels::getLogChannel(const String& name)
{
    return WTFLogChannelByName(m_logChannels.data(), m_logChannels.size(), name.utf8().data());
}

#endif // !LOG_DISABLED || !RELEASE_LOG_DISABLED

} // namespace WTF
