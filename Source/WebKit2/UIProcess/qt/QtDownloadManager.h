/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef QtDownloadManager_h
#define QtDownloadManager_h

#include <QMap>
#include <WKContext.h>

class QWebDownloadItem;

namespace WebCore {
class ResourceResponse;
}

namespace WebKit {

class DownloadProxy;
class QtWebError;
class WebContext;

class QtDownloadManager {
public:
    QtDownloadManager(WebContext*);
    ~QtDownloadManager();

    void addDownload(DownloadProxy*, QWebDownloadItem*);

private:
    void downloadReceivedResponse(DownloadProxy*, const WebCore::ResourceResponse&);
    void downloadCreatedDestination(DownloadProxy*, const QString& path);
    void downloadFinished(DownloadProxy*);
    void downloadFailed(DownloadProxy*, const QtWebError&);
    void downloadDataReceived(DownloadProxy*, uint64_t length);

    // WKContextDownloadClient callbacks.
    static void didReceiveResponse(WKContextRef, WKDownloadRef, WKURLResponseRef, const void* clientInfo);
    static void didCreateDestination(WKContextRef, WKDownloadRef, WKStringRef path, const void* clientInfo);
    static void didFinishDownload(WKContextRef, WKDownloadRef, const void* clientInfo);
    static void didFailDownload(WKContextRef, WKDownloadRef, WKErrorRef, const void* clientInfo);
    static void didReceiveDataForDownload(WKContextRef, WKDownloadRef, uint64_t length, const void* clientInfo);

    QMap<uint64_t, QWebDownloadItem*> m_downloads;
};

} // namespace WebKit

#endif /* QtDownloadManager_h */
