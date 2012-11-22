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
#ifndef QWebFrameAdapter_h
#define QWebFrameAdapter_h

#include "FrameLoaderClientQt.h"
#include "PlatformEvent.h"

#include <FrameView.h>
#include <IntRect.h>
#include <IntSize.h>
#include <KURL.h>

#include <QNetworkAccessManager>
#include <QSize>

namespace WebCore {
class Frame;
}

QT_BEGIN_NAMESPACE
class QPoint;
QT_END_NAMESPACE

#if ENABLE(GESTURE_EVENTS)
class QGestureEventFacade;
#endif
class QWebFrame;
class QWebPageAdapter;

class QWebFrameData {
public:
    QWebFrameData(WebCore::Page*, WebCore::Frame* parentFrame = 0, WebCore::HTMLFrameOwnerElement* = 0, const WTF::String& frameName = WTF::String());

    WTF::String name;
    WebCore::HTMLFrameOwnerElement* ownerElement;
    WebCore::Page* page;
    RefPtr<WebCore::Frame> frame;
    WebCore::FrameLoaderClientQt* frameLoaderClient;

    WTF::String referrer;
    bool allowsScrolling;
    int marginWidth;
    int marginHeight;
};

class QWebFrameAdapter {
public:
    static QUrl ensureAbsoluteUrl(const QUrl&);

    QWebFrameAdapter();
    virtual ~QWebFrameAdapter();

    virtual QWebFrame* apiHandle() = 0;
    virtual QObject* handle() = 0;
    virtual void contentsSizeDidChange(const QSize&) = 0;
    virtual int scrollBarPolicy(Qt::Orientation) const = 0;
    virtual void emitUrlChanged() = 0;
    virtual void didStartProvisionalLoad() = 0;
    virtual void didClearWindowObject() = 0;
    virtual bool handleProgressFinished(QPoint*) = 0;
    virtual void emitInitialLayoutCompleted() = 0;
    virtual void emitIconChanged() = 0;
    virtual void emitLoadStarted(bool originatingLoad) = 0;
    virtual void emitLoadFinished(bool originatingLoad, bool ok) = 0;
    virtual QWebFrameAdapter* createChildFrame(QWebFrameData*) = 0;

    void load(const QNetworkRequest&, QNetworkAccessManager::Operation = QNetworkAccessManager::GetOperation, const QByteArray& body = QByteArray());
    inline bool hasView() const { return frame && frame->view(); }
#if ENABLE(GESTURE_EVENTS)
    void handleGestureEvent(QGestureEventFacade*);
#endif
    QWebFrameAdapter* createFrame(QWebFrameData*);

    WebCore::IntSize scrollPosition() const;
    WebCore::IntRect frameRect() const;
    QWebPageAdapter* pageAdapter;

// protected:
    bool allowsScrolling;
    int marginWidth;
    int marginHeight;

// private:
    void init(QWebPageAdapter*);
    void init(QWebPageAdapter*, QWebFrameData*);

    WebCore::Frame *frame;
    WebCore::FrameLoaderClientQt *frameLoaderClient;
    WebCore::KURL url;

    static QWebFrameAdapter* kit(const WebCore::Frame*);

//    friend class ChromeClientQt;
};

#endif // QWebFrameAdapter_h
