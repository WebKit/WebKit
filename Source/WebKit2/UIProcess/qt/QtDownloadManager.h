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
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

class QtWebError;
class QWebDownloadItem;

namespace WebCore {
class ResourceResponse;
}

namespace WebKit {

class DownloadProxy;
class WebContext;

class QtDownloadManager : public RefCounted<QtDownloadManager> {
public:
    ~QtDownloadManager();

    void addDownload(DownloadProxy*, QWebDownloadItem*);

    void downloadReceivedResponse(DownloadProxy*, const WebCore::ResourceResponse&);
    void downloadCreatedDestination(DownloadProxy*, const QString& path);
    void downloadFinished(DownloadProxy*);
    void downloadFailed(DownloadProxy*, const QtWebError&);
    void downloadDataReceived(DownloadProxy*, uint64_t length);

    static PassRefPtr<QtDownloadManager> create(WebContext*);
private:
    QtDownloadManager();

    QMap<uint64_t, QWebDownloadItem*> m_downloads;
};

}
#endif /* QtDownloadManager_h */
