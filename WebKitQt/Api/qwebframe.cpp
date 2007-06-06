/*
    Copyright (C) 2007 Trolltech ASA
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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/
#include "qwebframe.h"
#include "qwebpage.h"
#include "qwebpage_p.h"
#include "qwebframe_p.h"

#include "FrameLoaderClientQt.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformWheelEvent.h"
#include "ResourceRequest.h"

#include "markup.h"
#include "RenderTreeAsText.h"
#include "Element.h"
#include "Document.h"
#include "DragData.h"
#include "RenderObject.h"

#include "bindings/runtime.h"
#include "bindings/runtime_root.h"
#include "ExecState.h"
#include "object.h"

#include "wtf/HashMap.h"

#include <qpainter.h>
#include <qevent.h>
#include <qscrollbar.h>
#include <qdebug.h>

using namespace WebCore;

void QWebFramePrivate::init(QWebFrame *qframe, WebCore::Page *page, QWebFrameData *frameData)
{
    q = qframe;

    q->setLineWidth(0);
    q->setMidLineWidth(0);
    q->setFrameShape(QFrame::NoFrame);
    q->setMouseTracking(true);
    q->setFocusPolicy(Qt::StrongFocus);
    q->verticalScrollBar()->setSingleStep(20);
    q->horizontalScrollBar()->setSingleStep(20);

    frameLoaderClient = new FrameLoaderClientQt();
    frame = new Frame(page, frameData->ownerElement, frameLoaderClient);
    frameLoaderClient->setFrame(qframe, frame.get());

    frameView = new FrameView(frame.get());
    frameView->deref();
    frameView->setScrollArea(qframe);
    frameView->setAllowsScrolling(frameData->allowsScrolling);
    frame->setView(frameView.get());
    frame->init();
    eventHandler = frame->eventHandler();
}

void QWebFramePrivate::_q_adjustScrollbars()
{
    QAbstractSlider *hbar = q->horizontalScrollBar();
    QAbstractSlider *vbar = q->verticalScrollBar();

    const QSize viewportSize = q->viewport()->size();
    QSize docSize = QSize(frameView->contentsWidth(), frameView->contentsHeight());

    hbar->setRange(0, docSize.width() - viewportSize.width());
    hbar->setPageStep(viewportSize.width());

    vbar->setRange(0, docSize.height() - viewportSize.height());
    vbar->setPageStep(viewportSize.height());
}

void QWebFramePrivate::_q_handleKeyEvent(QKeyEvent *ev, bool isKeyUp)
{
    PlatformKeyboardEvent kevent(ev, isKeyUp);

    if (!eventHandler)
        return;

    bool handled = eventHandler->keyEvent(kevent);

    ev->setAccepted(handled);
}

QWebFrame::QWebFrame(QWebPage *parent, QWebFrameData *frameData)
    : QAbstractScrollArea(parent)
    , d(new QWebFramePrivate)
{
    d->page = parent;
    d->init(this, parent->d->page, frameData);

    if (!frameData->url.isEmpty()) {
        ResourceRequest request(frameData->url, frameData->referrer);
        d->frame->loader()->load(request, frameData->name);
    }
}


QWebFrame::QWebFrame(QWebFrame *parent, QWebFrameData *frameData)
    : QAbstractScrollArea(parent->viewport())
    , d(new QWebFramePrivate)
{
    QPalette pal = palette();
    pal.setBrush(QPalette::Background, Qt::white);
    setPalette(pal);

    d->page = parent->d->page;
    d->init(this, parent->d->page->d->page, frameData);
}

QWebFrame::~QWebFrame()
{
    Q_ASSERT(d->frame == 0);
    Q_ASSERT(d->frameView == 0);
    delete d;
}

void QWebFrame::addToJSWindowObject(const QByteArray &name, QObject *object)
{
    KJS::Bindings::RootObject *root = d->frame->bindingRootObject();
    KJS::ExecState *exec = root->interpreter()->globalExec();
    KJS::JSObject *rootObject = root->interpreter()->globalObject();
    KJS::JSObject *window = rootObject->get(exec, KJS::Identifier("window"))->getObject();
    if (!window) {
        qDebug() << "Warning: couldn't get window object";
        return;
    }

    KJS::JSObject *testController =
        KJS::Bindings::Instance::createRuntimeObject(KJS::Bindings::Instance::QtLanguage,
                                                     object, root);

    window->put(exec, KJS::Identifier(name.constData()), testController);
}


QString QWebFrame::markup() const
{
    if (!d->frame->document())
        return QString();
    return createMarkup(d->frame->document());
}

QString QWebFrame::innerText() const
{
    if (d->frameView->layoutPending())
        d->frameView->layout();

    Element *documentElement = d->frame->document()->documentElement();
    return documentElement->innerText();
}

QString QWebFrame::renderTreeDump() const
{
    if (d->frameView->layoutPending())
        d->frameView->layout();

    return externalRepresentation(d->frame->renderer());
}

QString QWebFrame::title() const
{
    if (d->frame->document())
        return d->frame->document()->title();
    else return QString();
}

QWebPage * QWebFrame::page() const
{
    return d->page;
}

QString QWebFrame::selectedText() const
{
    return d->frame->selectedText();
}

void QWebFrame::resizeEvent(QResizeEvent *e)
{
    QAbstractScrollArea::resizeEvent(e);
    if (d->frame && d->frameView) {
        d->frame->forceLayout();
        d->frame->view()->adjustViewSize();
    }
    d->_q_adjustScrollbars();
}

QList<QWebFrame*> QWebFrame::childFrames() const
{
    QList<QWebFrame*> rc;
    if (d->frame) {
        FrameTree *tree = d->frame->tree();
        for (Frame *child = tree->firstChild(); child; child = child->tree()->nextSibling()) {
            FrameLoader *loader = child->loader();
            FrameLoaderClientQt *client = static_cast<FrameLoaderClientQt*>(loader->client());
            if (client)
                rc.append(client->webFrame());
        }

    }
    return rc;
}

void QWebFrame::paintEvent(QPaintEvent *ev)
{
    if (!d->frameView || !d->frame->renderer())
        return;

#ifdef QWEBKIT_TIME_RENDERING
    QTime time;
    time.start();
#endif
    QRect clip = ev->rect();

    if (d->frameView->needsLayout()) {
        d->frameView->layout();
    }
    QPainter p(viewport());
    GraphicsContext ctx(&p);

    const int xOffset = horizontalScrollBar()->value();
    const int yOffset = verticalScrollBar()->value();

    ctx.translate(-xOffset, -yOffset);
    clip.translate(xOffset, yOffset);

    d->frame->paint(&ctx, clip);
    p.end();

#ifdef    QWEBKIT_TIME_RENDERING
    int elapsed = time.elapsed();
    qDebug()<<"paint event on "<<clip<<", took to render =  "<<elapsed;
#endif
}

void QWebFrame::mouseMoveEvent(QMouseEvent *ev)
{
    if (!d->frameView)
        return;

    d->eventHandler->handleMouseMoveEvent(PlatformMouseEvent(ev, 0));
    const int xOffset = horizontalScrollBar()->value();
    const int yOffset = verticalScrollBar()->value();
    IntPoint pt(ev->x() + xOffset, ev->y() + yOffset);
    WebCore::HitTestResult result = d->eventHandler->hitTestResultAtPoint(pt, false);
    WebCore::Element *link = result.URLElement();
    if (link != d->lastHoverElement) {
        d->lastHoverElement = link;
        emit hoveringOverLink(result.absoluteLinkURL().prettyURL(), result.title());
    }
}

void QWebFrame::mousePressEvent(QMouseEvent *ev)
{
    if (!d->eventHandler)
        return;

    d->eventHandler->handleMousePressEvent(PlatformMouseEvent(ev, 1));
}

void QWebFrame::mouseReleaseEvent(QMouseEvent *ev)
{
    if (!d->frameView)
        return;

    d->eventHandler->handleMouseReleaseEvent(PlatformMouseEvent(ev, 0));
}

void QWebFrame::wheelEvent(QWheelEvent *e)
{
    PlatformWheelEvent wkEvent(e);
    bool accepted = false;
    if (d->eventHandler)
        accepted = d->eventHandler->handleWheelEvent(wkEvent);

    e->setAccepted(accepted);
    if (!accepted)
        QAbstractScrollArea::wheelEvent(e);
}

void QWebFrame::keyPressEvent(QKeyEvent *ev)
{
    d->_q_handleKeyEvent(ev, false);
}

void QWebFrame::keyReleaseEvent(QKeyEvent *ev)
{
    d->_q_handleKeyEvent(ev, true);
}

/*!\reimp
*/
void QWebFrame::scrollContentsBy(int dx, int dy)
{
    viewport()->scroll(dx, dy);
}
