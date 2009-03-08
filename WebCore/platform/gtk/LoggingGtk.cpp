/*
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009 Gustavo Noronha Silva <gns@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "Logging.h"

#include <glib.h>
#include <string.h>

namespace WebCore {

// Inspired by the code used by the Qt port

static WTFLogChannel* getChannelFromName(const char* channelName)
{
    if (strlen(channelName) < 3)
        return 0;

    if (!g_ascii_strcasecmp(channelName, "BackForward"))
        return &LogBackForward;
    if (!g_ascii_strcasecmp(channelName, "Editing"))
        return &LogEditing;
    if (!g_ascii_strcasecmp(channelName, "Events"))
        return &LogEvents;
    if (!g_ascii_strcasecmp(channelName, "Frames"))
        return &LogFrames;
    if (!g_ascii_strcasecmp(channelName, "FTP"))
        return &LogFTP;
    if (!g_ascii_strcasecmp(channelName, "History"))
        return &LogHistory;
    if (!g_ascii_strcasecmp(channelName, "IconDatabase"))
        return &LogIconDatabase;
    if (!g_ascii_strcasecmp(channelName, "Loading"))
        return &LogLoading;
    if (!g_ascii_strcasecmp(channelName, "Media"))
        return &LogMedia;
    if (!g_ascii_strcasecmp(channelName, "Network"))
        return &LogNetwork;
    if (!g_ascii_strcasecmp(channelName, "NotYetImplemented"))
        return &LogNotYetImplemented;
    if (!g_ascii_strcasecmp(channelName, "PageCache"))
        return &LogPageCache;
    if (!g_ascii_strcasecmp(channelName, "PlatformLeaks"))
        return &LogPlatformLeaks;
    if (!g_ascii_strcasecmp(channelName, "Plugin"))
        return &LogPlugin;
    if (!g_ascii_strcasecmp(channelName, "PopupBlocking"))
        return &LogPopupBlocking;
    if (!g_ascii_strcasecmp(channelName, "SpellingAndGrammar"))
        return &LogSpellingAndGrammar;
    if (!g_ascii_strcasecmp(channelName, "SQLDatabase"))
        return &LogSQLDatabase;
    if (!g_ascii_strcasecmp(channelName, "StorageAPI"))
        return &LogStorageAPI;
    if (!g_ascii_strcasecmp(channelName, "TextConversion"))
        return &LogTextConversion;
    if (!g_ascii_strcasecmp(channelName, "Threading"))
        return &LogThreading;

    return 0;
}

void InitializeLoggingChannelsIfNecessary()
{
    static bool didInitializeLoggingChannels = false;
    if (didInitializeLoggingChannels)
        return;

    didInitializeLoggingChannels = true;

    char* logEnv = getenv("WEBKIT_DEBUG");
    if (!logEnv)
        return;

    // we set up the logs anyway because some of our logging, such as
    // soup's is available in release builds
#if defined(NDEBUG)
    g_warning("WEBKIT_DEBUG is not empty, but this is a release build. Notice that many log messages will only appear in a debug build.");
#endif

    char** logv = g_strsplit(logEnv, " ", -1);

    for (int i = 0; logv[i]; i++) {
        WTFLogChannel* channel = getChannelFromName(logv[i]);
        if (!channel)
            continue;
        channel->state = WTFLogChannelOn;
    }

    g_strfreev(logv);

    // to disable logging notImplemented set the DISABLE_NI_WARNING
    // environment variable to 1
    LogNotYetImplemented.state = WTFLogChannelOn;
}

} // namespace WebCore
