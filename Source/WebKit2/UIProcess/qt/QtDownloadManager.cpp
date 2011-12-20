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

#include "config.h"
#include "QtDownloadManager.h"

#include "DownloadProxy.h"
#include "QtWebError.h"
#include "WKStringQt.h"
#include "WKURLQt.h"
#include "WebContext.h"
#include "qwebdownloaditem_p.h"
#include "qwebdownloaditem_p_p.h"

namespace WebKit {

QtDownloadManager::QtDownloadManager(WebContext* context)
{
    WKContextDownloadClient downloadClient;
    memset(&downloadClient, 0, sizeof(WKContextDownloadClient));
    downloadClient.version = kWKContextDownloadClientCurrentVersion;
    downloadClient.clientInfo = this;
    downloadClient.didReceiveResponse = didReceiveResponse;
    downloadClient.didReceiveData = didReceiveDataForDownload;
    downloadClient.didCreateDestination = didCreateDestination;
    downloadClient.didFinish = didFinishDownload;
    downloadClient.didFail = didFailDownload;
    WKContextSetDownloadClient(toAPI(context), &downloadClient);
}

QtDownloadManager::~QtDownloadManager()
{
}

void QtDownloadManager::addDownload(DownloadProxy* download, QWebDownloadItem* downloadItem)
{
    m_downloads[download->downloadID()] = downloadItem;
}

void QtDownloadManager::downloadReceivedResponse(DownloadProxy* download, const WebCore::ResourceResponse& response)
{
    // Will be called when the headers are read by WebProcess.
    QWebDownloadItem* downloadItem = m_downloads.value(download->downloadID());
    ASSERT(downloadItem);

    downloadItem->d->sourceUrl = response.url();
    downloadItem->d->mimeType = response.mimeType();
    downloadItem->d->expectedContentLength = response.expectedContentLength();
    downloadItem->d->suggestedFilename = response.suggestedFilename();

    downloadItem->d->didReceiveResponse(downloadItem);
}

void QtDownloadManager::downloadCreatedDestination(DownloadProxy* download, const QString& path)
{
    QWebDownloadItem* downloadItem = m_downloads.value(download->downloadID());
    ASSERT(downloadItem);
    downloadItem->d->destinationPath = path;
    emit downloadItem->destinationFileCreated(downloadItem->d->destinationPath);
}

void QtDownloadManager::downloadFinished(DownloadProxy* download)
{
    // Will be called when download finishes with success.
    QWebDownloadItem* downloadItem = m_downloads.take(download->downloadID());
    ASSERT(downloadItem);
    emit downloadItem->succeeded();
}

void QtDownloadManager::downloadFailed(DownloadProxy* download, const QtWebError& error)
{
    // Will be called when download fails or is aborted.
    QWebDownloadItem* downloadItem = m_downloads.take(download->downloadID());
    ASSERT(downloadItem);

    // If the parent is null at this point, the download failed before it
    // received a response and downloadRequested was emitted.
    // Due to this the item will never be parented and we have to delete it
    // manually at this point.
    if (!downloadItem->parent()) {
        delete downloadItem;
        return;
    }

    emit downloadItem->failed(error.errorCodeAsDownloadError(), error.url(), error.description());
}

void QtDownloadManager::downloadDataReceived(DownloadProxy* download, uint64_t length)
{
    // Will be called everytime bytes were written to destination file by WebProcess.
    QWebDownloadItem* downloadItem = m_downloads.value(download->downloadID());
    ASSERT(downloadItem);
    downloadItem->d->totalBytesReceived += length;
    emit downloadItem->totalBytesReceivedChanged(length);
}

static inline QtDownloadManager* toQtDownloadManager(const void* clientInfo)
{
    ASSERT(clientInfo);
    return reinterpret_cast<QtDownloadManager*>(const_cast<void*>(clientInfo));
}

void QtDownloadManager::didReceiveResponse(WKContextRef, WKDownloadRef download, WKURLResponseRef response, const void *clientInfo)
{
    toQtDownloadManager(clientInfo)->downloadReceivedResponse(toImpl(download), toImpl(response)->resourceResponse());
}

void QtDownloadManager::didCreateDestination(WKContextRef, WKDownloadRef download, WKStringRef path, const void *clientInfo)
{
    toQtDownloadManager(clientInfo)->downloadCreatedDestination(toImpl(download), WKStringCopyQString(path));
}

void QtDownloadManager::didFinishDownload(WKContextRef, WKDownloadRef download, const void *clientInfo)
{
    toQtDownloadManager(clientInfo)->downloadFinished(toImpl(download));
}

void QtDownloadManager::didFailDownload(WKContextRef, WKDownloadRef download, WKErrorRef error, const void *clientInfo)
{
    toQtDownloadManager(clientInfo)->downloadFailed(toImpl(download), QtWebError(error));
}

void QtDownloadManager::didReceiveDataForDownload(WKContextRef, WKDownloadRef download, uint64_t length, const void *clientInfo)
{
    toQtDownloadManager(clientInfo)->downloadDataReceived(toImpl(download), length);
}

}
