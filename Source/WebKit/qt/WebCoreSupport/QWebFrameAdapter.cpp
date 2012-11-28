/*
 * Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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
 *
 */

#include "config.h"
#include "QWebFrameAdapter.h"

#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoaderClientQt.h"
#include "QWebPageAdapter.h"
#if ENABLE(GESTURE_EVENTS)
#include "PlatformGestureEvent.h"
#include "WebEventConversion.h"
#endif

#include <QFileInfo>
#include <QNetworkRequest>

using namespace WebCore;

static inline ResourceRequestCachePolicy cacheLoadControlToCachePolicy(uint cacheLoadControl)
{
    switch (cacheLoadControl) {
    case QNetworkRequest::AlwaysNetwork:
        return WebCore::ReloadIgnoringCacheData;
    case QNetworkRequest::PreferCache:
        return WebCore::ReturnCacheDataElseLoad;
    case QNetworkRequest::AlwaysCache:
        return WebCore::ReturnCacheDataDontLoad;
    default:
        break;
    }
    return WebCore::UseProtocolCachePolicy;
}

QWebFrameData::QWebFrameData(WebCore::Page* parentPage, WebCore::Frame* parentFrame, WebCore::HTMLFrameOwnerElement* ownerFrameElement, const WTF::String& frameName)
    : name(frameName)
    , ownerElement(ownerFrameElement)
    , page(parentPage)
    , allowsScrolling(true)
    , marginWidth(0)
    , marginHeight(0)
{
    frameLoaderClient = new FrameLoaderClientQt();
    frame = Frame::create(page, ownerElement, frameLoaderClient);

    // FIXME: All of the below should probably be moved over into WebCore
    frame->tree()->setName(name);
    if (parentFrame)
        parentFrame->tree()->appendChild(frame);
}

QWebFrameAdapter::QWebFrameAdapter()
    : pageAdapter(0)
    , allowsScrolling(true)
    , marginWidth(-1)
    , marginHeight(-1)
    , frame(0)
    , frameLoaderClient(0)
{
}

QWebFrameAdapter::~QWebFrameAdapter()
{
    if (frameLoaderClient)
        frameLoaderClient->m_webFrame = 0;
}

void QWebFrameAdapter::load(const QNetworkRequest& req, QNetworkAccessManager::Operation operation, const QByteArray& body)
{
    if (frame->tree()->parent())
        pageAdapter->insideOpenCall = true;

    QUrl url = ensureAbsoluteUrl(req.url());

    WebCore::ResourceRequest request(url);

    switch (operation) {
    case QNetworkAccessManager::HeadOperation:
        request.setHTTPMethod("HEAD");
        break;
    case QNetworkAccessManager::GetOperation:
        request.setHTTPMethod("GET");
        break;
    case QNetworkAccessManager::PutOperation:
        request.setHTTPMethod("PUT");
        break;
    case QNetworkAccessManager::PostOperation:
        request.setHTTPMethod("POST");
        break;
    case QNetworkAccessManager::DeleteOperation:
        request.setHTTPMethod("DELETE");
        break;
    case QNetworkAccessManager::CustomOperation:
        request.setHTTPMethod(req.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray().constData());
        break;
    case QNetworkAccessManager::UnknownOperation:
        // eh?
        break;
    }

    QVariant cacheLoad = req.attribute(QNetworkRequest::CacheLoadControlAttribute);
    if (cacheLoad.isValid()) {
        bool ok;
        uint cacheLoadValue = cacheLoad.toUInt(&ok);
        if (ok)
            request.setCachePolicy(cacheLoadControlToCachePolicy(cacheLoadValue));
    }

    QList<QByteArray> httpHeaders = req.rawHeaderList();
    for (int i = 0; i < httpHeaders.size(); ++i) {
        const QByteArray &headerName = httpHeaders.at(i);
        request.addHTTPHeaderField(QString::fromLatin1(headerName), QString::fromLatin1(req.rawHeader(headerName)));
    }

    if (!body.isEmpty())
        request.setHTTPBody(WebCore::FormData::create(body.constData(), body.size()));

    frame->loader()->load(WebCore::FrameLoadRequest(frame, request));

    if (frame->tree()->parent())
        pageAdapter->insideOpenCall = false;
}

#if ENABLE(GESTURE_EVENTS)
void QWebFrameAdapter::handleGestureEvent(QGestureEventFacade* gestureEvent)
{
    ASSERT(hasView());
    switch (gestureEvent->type) {
    case Qt::TapGesture:
        frame->eventHandler()->handleGestureEvent(convertGesture(gestureEvent));
        break;
    case Qt::TapAndHoldGesture:
        frame->eventHandler()->sendContextMenuEventForGesture(convertGesture(gestureEvent));
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}
#endif

WebCore::IntSize QWebFrameAdapter::scrollPosition() const
{
    if (!hasView())
        return IntSize();
    return frame->view()->scrollOffset();
}

IntRect QWebFrameAdapter::frameRect() const
{
    if (!hasView())
        return IntRect();
    return frame->view()->frameRect();
}

void QWebFrameAdapter::init(QWebPageAdapter* pageAdapter)
{
    QWebFrameData frameData(pageAdapter->page);
    init(pageAdapter, &frameData);
}

void QWebFrameAdapter::init(QWebPageAdapter* pageAdapter, QWebFrameData* frameData)
{
    this->pageAdapter = pageAdapter;
    allowsScrolling = frameData->allowsScrolling;
    marginWidth = frameData->marginWidth;
    marginHeight = frameData->marginHeight;
    frame = frameData->frame.get();
    frameLoaderClient = frameData->frameLoaderClient;
    frameLoaderClient->setFrame(this, frame);

    frame->init();
}

QWebFrameAdapter* QWebFrameAdapter::kit(const Frame* frame)
{
    return static_cast<FrameLoaderClientQt*>(frame->loader()->client())->webFrame();
}

QUrl QWebFrameAdapter::ensureAbsoluteUrl(const QUrl& url)
{
    if (!url.isValid() || !url.isRelative())
        return url;

    // This contains the URL with absolute path but without
    // the query and the fragment part.
    QUrl baseUrl = QUrl::fromLocalFile(QFileInfo(url.toLocalFile()).absoluteFilePath());

    // The path is removed so the query and the fragment parts are there.
    QString pathRemoved = url.toString(QUrl::RemovePath);
    QUrl toResolve(pathRemoved);

    return baseUrl.resolved(toResolve);
}
