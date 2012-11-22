/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2007 Staikos Computing Services Inc.

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

#ifndef QWEBFRAME_P_H
#define QWEBFRAME_P_H

#include "QWebFrameAdapter.h"

#include "qwebframe.h"
#include "qwebpage_p.h"

#include "EventHandler.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "KURL.h"
#if ENABLE(ORIENTATION_EVENTS)
#include "qorientationsensor.h"
#endif // ENABLE(ORIENTATION_EVENTS).
#include "qwebelement.h"
#if USE(ACCELERATED_COMPOSITING)
#include "texmap/TextureMapper.h"
#endif
#include "ViewportArguments.h"
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>


namespace WebCore {
    class FrameLoaderClientQt;
    class FrameView;
    class HTMLFrameOwnerElement;
    class Scrollbar;
    class TextureMapperLayer;
}
class QWebPage;

class QWebFramePrivate : public QWebFrameAdapter {
public:
    QWebFramePrivate()
        : q(0)
        , horizontalScrollBarPolicy(Qt::ScrollBarAsNeeded)
        , verticalScrollBarPolicy(Qt::ScrollBarAsNeeded)
        , page(0)
#if USE(ACCELERATED_COMPOSITING)
        , rootTextureMapperLayer(0)
#endif
        {}
    void setPage(QWebPage*);

    inline QWebFrame *parentFrame() { return qobject_cast<QWebFrame*>(q->parent()); }

    WebCore::Scrollbar* horizontalScrollBar() const;
    WebCore::Scrollbar* verticalScrollBar() const;

    static WebCore::Frame* core(const QWebFrame*);
    static QWebFrame* kit(const WebCore::Frame*);
    static QWebFrame* kit(const QWebFrameAdapter*);

    void renderRelativeCoords(WebCore::GraphicsContext*, QFlags<QWebFrame::RenderLayer>, const QRegion& clip);
#if USE(TILED_BACKING_STORE)
    void renderFromTiledBackingStore(WebCore::GraphicsContext*, const QRegion& clip);
#endif

#if USE(ACCELERATED_COMPOSITING)
    void renderCompositedLayers(WebCore::GraphicsContext*, const WebCore::IntRect& clip);
#endif
    void renderFrameExtras(WebCore::GraphicsContext*, QFlags<QWebFrame::RenderLayer>, const QRegion& clip);
    void _q_orientationChanged();


    // Adapter implementation
    virtual QWebFrame* apiHandle() OVERRIDE;
    virtual QObject* handle() OVERRIDE;
    virtual void contentsSizeDidChange(const QSize &) OVERRIDE;
    virtual int scrollBarPolicy(Qt::Orientation) const OVERRIDE;
    virtual void emitUrlChanged() OVERRIDE;
    virtual void didStartProvisionalLoad() OVERRIDE;
    virtual void didClearWindowObject() OVERRIDE;
    virtual bool handleProgressFinished(QPoint*) OVERRIDE;
    virtual void emitInitialLayoutCompleted() OVERRIDE;
    virtual void emitIconChanged() OVERRIDE;
    virtual void emitLoadStarted(bool originatingLoad) OVERRIDE;
    virtual void emitLoadFinished(bool originatingLoad, bool ok) OVERRIDE;
    virtual QWebFrameAdapter* createChildFrame(QWebFrameData*) OVERRIDE;

    QWebFrame *q;
    Qt::ScrollBarPolicy horizontalScrollBarPolicy;
    Qt::ScrollBarPolicy verticalScrollBarPolicy;
    QWebPage *page;

#if USE(ACCELERATED_COMPOSITING)
    WebCore::TextureMapperLayer* rootTextureMapperLayer;
    OwnPtr<WebCore::TextureMapper> textureMapper;
#endif

#if ENABLE(ORIENTATION_EVENTS)
    QOrientationSensor m_orientation;
#endif // ENABLE(ORIENTATION_EVENTS).
};

class QWebHitTestResultPrivate {
public:
    QWebHitTestResultPrivate() : isContentEditable(false), isContentSelected(false), isScrollBar(false) {}
    QWebHitTestResultPrivate(const WebCore::HitTestResult &hitTest);

    QPoint pos;
    QRect boundingRect;
    QWebElement enclosingBlock;
    QString title;
    QString linkText;
    QUrl linkUrl;
    QString linkTitle;
    QPointer<QWebFrame> linkTargetFrame;
    QWebElement linkElement;
    QString alternateText;
    QUrl imageUrl;
    QPixmap pixmap;
    bool isContentEditable;
    bool isContentSelected;
    bool isScrollBar;
    QPointer<QWebFrame> frame;
    RefPtr<WebCore::Node> innerNode;
    RefPtr<WebCore::Node> innerNonSharedNode;
};

#endif
