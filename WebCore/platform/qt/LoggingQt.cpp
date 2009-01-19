/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "Logging.h"

#include <QDebug>
#include <QStringList>

namespace WebCore {

#if !defined(NDEBUG)
static WTFLogChannel* getChannelFromName(const QString& channelName)
{
    if (!channelName.length() >= 2)
        return 0;

    if (channelName == QLatin1String("BackForward")) return &LogBackForward;
    if (channelName == QLatin1String("Editing")) return &LogEditing;
    if (channelName == QLatin1String("Events")) return &LogEvents;
    if (channelName == QLatin1String("Frames")) return &LogFrames;
    if (channelName == QLatin1String("FTP")) return &LogFTP;
    if (channelName == QLatin1String("History")) return &LogHistory;
    if (channelName == QLatin1String("IconDatabase")) return &LogIconDatabase;
    if (channelName == QLatin1String("Loading")) return &LogLoading;
    if (channelName == QLatin1String("Media")) return &LogMedia;
    if (channelName == QLatin1String("Network")) return &LogNetwork;
    if (channelName == QLatin1String("NotYetImplemented")) return &LogNotYetImplemented;
    if (channelName == QLatin1String("PageCache")) return &LogPageCache;
    if (channelName == QLatin1String("PlatformLeaks")) return &LogPlatformLeaks;
    if (channelName == QLatin1String("Plugin")) return &LogPlugin;
    if (channelName == QLatin1String("PopupBlocking")) return &LogPopupBlocking;
    if (channelName == QLatin1String("SpellingAndGrammar")) return &LogSpellingAndGrammar;
    if (channelName == QLatin1String("SQLDatabase")) return &LogSQLDatabase;
    if (channelName == QLatin1String("StorageAPI")) return &LogStorageAPI;
    if (channelName == QLatin1String("TextConversion")) return &LogTextConversion;
    if (channelName == QLatin1String("Threading")) return &LogThreading;

    return 0;
}
#endif

void InitializeLoggingChannelsIfNecessary()
{
    static bool haveInitializedLoggingChannels = false;
    if (haveInitializedLoggingChannels)
        return;

    haveInitializedLoggingChannels = true;

    QByteArray loggingEnv = qgetenv("QT_WEBKIT_LOG");
    if (loggingEnv.isEmpty())
        return;

#if defined(NDEBUG)
    qWarning("This is a release build. Setting QT_WEBKIT_LOG will have no effect.");
#else
    QList<QByteArray> channels = loggingEnv.split(',');
    QListIterator<QByteArray> iter(channels);

    while (iter.hasNext()) {
        QByteArray channelName = iter.next();
        WTFLogChannel* channel = getChannelFromName(channelName);
        if (!channel) continue;
        channel->state = WTFLogChannelOn;
    }

    // By default we log calls to notImplemented(). This can be turned
    // off by setting the environment variable DISABLE_NI_WARNING to 1
    LogNotYetImplemented.state = WTFLogChannelOn;
#endif
}

} // namespace WebCore
