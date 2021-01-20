/*
 * Copyright (C) 2010, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Samsung Electronics
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Logging.h"
#include "LogInitialization.h"

#include <wtf/text/CString.h>

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED

#define DEFINE_WEBKIT2_LOG_CHANNEL(name) DEFINE_LOG_CHANNEL(name, LOG_CHANNEL_WEBKIT_SUBSYSTEM)
WEBKIT2_LOG_CHANNELS(DEFINE_WEBKIT2_LOG_CHANNEL)

static WTFLogChannel* logChannels[] = {
    WEBKIT2_LOG_CHANNELS(LOG_CHANNEL_ADDRESS)
};

namespace WebKit {

static const size_t logChannelCount = WTF_ARRAY_LENGTH(logChannels);
static bool logChannelsNeedInitialization = true;

void initializeLogChannelsIfNecessary(Optional<String> logChannelString)
{
    if (!logChannelsNeedInitialization && !logChannelString)
        return;

    logChannelsNeedInitialization = false;

    String enabledChannelsString = logChannelString ? logChannelString.value() : logLevelString();
    WTFInitializeLogChannelStatesFromString(logChannels, logChannelCount, enabledChannelsString.utf8().data());
}

WTFLogChannel* getLogChannel(const String& name)
{
    return WTFLogChannelByName(logChannels, logChannelCount, name.utf8().data());
}

} // namespace WebKit

#endif // !LOG_DISABLED || !RELEASE_LOG_DISABLED
