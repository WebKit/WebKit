/*
 * Copyright (C) 2009, 2010, 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "Logging.h"

namespace WebCore {

static inline void initializeWithUserDefault(WTFLogChannel& channel, bool enabled)
{
    if (enabled)
        channel.state = WTFLogChannelOn;
    else
        channel.state = WTFLogChannelOff;
}

void InitializeLoggingChannelsIfNecessary()
{
    static bool haveInitializedLoggingChannels = false;
    if (haveInitializedLoggingChannels)
        return;
    haveInitializedLoggingChannels = true;

    initializeWithUserDefault(LogNotYetImplemented, true);
    initializeWithUserDefault(LogFrames, true);
    initializeWithUserDefault(LogLoading, true);
    initializeWithUserDefault(LogPopupBlocking, true);
    initializeWithUserDefault(LogEvents, true);
    initializeWithUserDefault(LogEditing, true);
    initializeWithUserDefault(LogLiveConnect, true);
    initializeWithUserDefault(LogIconDatabase, false);
    initializeWithUserDefault(LogSQLDatabase, false);
    initializeWithUserDefault(LogSpellingAndGrammar, true);
    initializeWithUserDefault(LogBackForward, true);
    initializeWithUserDefault(LogHistory, true);
    initializeWithUserDefault(LogPageCache, true);
    initializeWithUserDefault(LogPlatformLeaks, true);
    initializeWithUserDefault(LogNetwork, true);
    initializeWithUserDefault(LogFTP, true);
    initializeWithUserDefault(LogThreading, true);
    initializeWithUserDefault(LogStorageAPI, true);
    initializeWithUserDefault(LogMedia, true);
    initializeWithUserDefault(LogPlugins, true);
    initializeWithUserDefault(LogArchives, true);
    initializeWithUserDefault(LogProgress, false);
    initializeWithUserDefault(LogResourceLoading, false);
    initializeWithUserDefault(LogFileAPI, true);
}

} // namespace WebCore
