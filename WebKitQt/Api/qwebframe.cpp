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

#include "FocusController.h"
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
#include "SelectionController.h"
#include "PlatformScrollBar.h"

#include "markup.h"
#include "RenderTreeAsText.h"
#include "Element.h"
#include "Document.h"
#include "DragData.h"
#include "RenderObject.h"

#include "bindings/runtime.h"
#include "bindings/runtime_root.h"
#include "kjs_proxy.h"
#include "kjs_binding.h"
#include "ExecState.h"
#include "object.h"

#include "wtf/HashMap.h"

#include <qdebug.h>
#include <qevent.h>
#include <qpainter.h>
#include <qscrollbar.h>

using namespace WebCore;

void QWebFramePrivate::init(QWebFrame *qframe, WebCore::Page *page, QWebFrameData *frameData)
{
    q = qframe;

    q->setLineWidth(0);
    q->setMidLineWidth(0);
    q->setFrameShape(QFrame::NoFrame);
    q->setMouseTracking(true);
    q->setFocusPolicy(Qt::ClickFocus);
//     q->verticalScrollBar()->setSingleStep(20);
//     q->horizontalScrollBar()->setSingleStep(20);

    frameLoaderClient = new FrameLoaderClientQt();
    frame = new Frame(page, frameData->ownerElement, frameLoaderClient);
    frameLoaderClient->setFrame(qframe, frame.get());

    frameView = new FrameView(frame.get());
    frameView->deref();
    frameView->setScrollArea(qframe);
    if (!frameData->allowsScrolling)
        frameView->suppressScrollbars(true);
    frame->setView(frameView.get());
    frame->init();
    eventHandler = frame->eventHandler();
}

QWebFrame *QWebFramePrivate::parentFrame()
{
    return qobject_cast<QWebFrame*>(q->parentWidget());
}

WebCore::PlatformScrollbar *QWebFramePrivate::horizontalScrollBar() const
{
    Q_ASSERT(frameView);
    return frameView->horizontalScrollBar();
}

WebCore::PlatformScrollbar *QWebFramePrivate::verticalScrollBar() const
{
    Q_ASSERT(frameView);
    return frameView->verticalScrollBar();
}

QWebFrame::QWebFrame(QWebPage *parent, QWebFrameData *frameData)
    : QFrame(parent)
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
    : QFrame(parent)
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
    QFrame::resizeEvent(e);
    if (d->frame && d->frameView) {
        d->frame->forceLayout();
        d->frame->view()->adjustViewSize();
    }
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

void QWebFrame::suppressScrollbars(bool suppress)
{
    d->frameView->suppressScrollbars(suppress);
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

    QPainter p(this);

    GraphicsContext ctx(&p);
    d->frameView->paint(&ctx, clip);
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

    //WebCore expects all mouse events routed through mainFrame()
    if (this != d->page->mainFrame()) {
        ev->ignore();
        return;
    }

    d->eventHandler->handleMouseMoveEvent(PlatformMouseEvent(ev, 0));
    const int xOffset = d->horizontalScrollBar() ? d->horizontalScrollBar()->value() : 0;
    const int yOffset = d->verticalScrollBar() ? d->verticalScrollBar()->value() : 0;
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

    //WebCore expects all mouse events routed through mainFrame()
    if (this != d->page->mainFrame()) {
        ev->ignore();
        return;
    }

    if (ev->button() == Qt::RightButton)
        d->eventHandler->sendContextMenuEvent(PlatformMouseEvent(ev, 1));
    else
        d->eventHandler->handleMousePressEvent(PlatformMouseEvent(ev, 1));
    setFocus();
}

void QWebFrame::mouseDoubleClickEvent(QMouseEvent *ev)
{
    //WebCore expects all mouse events routed through mainFrame()
    if (!d->eventHandler)
        return;

    if (this != d->page->mainFrame()) {
        ev->ignore();
        return;
    }

    d->eventHandler->handleMousePressEvent(PlatformMouseEvent(ev, 2));
    setFocus();
}

void QWebFrame::mouseReleaseEvent(QMouseEvent *ev)
{
    //WebCore expects all mouse events routed through mainFrame()
    if (!d->frameView)
        return;

    if (this != d->page->mainFrame()) {
        ev->ignore();
        return;
    }

    d->eventHandler->handleMouseReleaseEvent(PlatformMouseEvent(ev, 0));
    setFocus();
}

void QWebFrame::wheelEvent(QWheelEvent *ev)
{
    //WebCore expects all mouse events routed through mainFrame()
    if (this != d->page->mainFrame()) {
        ev->ignore();
        return;
    }

    PlatformWheelEvent wkEvent(ev);
    bool accepted = false;
    if (d->eventHandler)
        accepted = d->eventHandler->handleWheelEvent(wkEvent);

    ev->setAccepted(accepted);
    if (!accepted)
        QFrame::wheelEvent(ev);
    setFocus();
}

void QWebFrame::keyPressEvent(QKeyEvent *ev)
{
    PlatformKeyboardEvent kevent(ev, false);

    if (!d->eventHandler)
        return;

    bool handled = d->eventHandler->keyEvent(kevent);
    if (handled) {
    } else {
        handled = true;
        PlatformScrollbar *h, *v;
        h = d->horizontalScrollBar();
        v = d->verticalScrollBar();
        if (!h || !v)
          return;

        switch (ev->key()) {
            case Qt::Key_Up:
                v->setValue(v->value() - 10);
                update();
                break;
            case Qt::Key_Down:
                v->setValue(v->value() + 10);
                update();
                break;
            case Qt::Key_Left:
                h->setValue(h->value() - 10);
                update();
                break;
            case Qt::Key_Right:
                h->setValue(h->value() + 10);
                update();
                break;
            case Qt::Key_PageUp:
                v->setValue(v->value() - height());
                update();
                break;
            case Qt::Key_PageDown:
                v->setValue(v->value() + height());
                update();
                break;
            default:
                handled = false;
                break;
        }
    }

    ev->setAccepted(handled);
}

void QWebFrame::keyReleaseEvent(QKeyEvent *ev)
{
    if (ev->isAutoRepeat()) {
        ev->setAccepted(true);
        return;
    }

    PlatformKeyboardEvent kevent(ev, true);

    if (!d->eventHandler)
        return;

    bool handled = d->eventHandler->keyEvent(kevent);
    ev->setAccepted(handled);
}

void QWebFrame::focusInEvent(QFocusEvent *ev)
{
    if (ev->reason() != Qt::PopupFocusReason) {
        d->frame->page()->focusController()->setFocusedFrame(d->frame);
        d->frame->setIsActive(true);
    }
    QFrame::focusInEvent(ev);
}

void QWebFrame::focusOutEvent(QFocusEvent *ev)
{
    QFrame::focusOutEvent(ev);
    if (ev->reason() != Qt::PopupFocusReason) {
        d->frame->selectionController()->clear();
        d->frame->setIsActive(false);
    }
}

bool QWebFrame::focusNextPrevChild(bool next)
{
    Q_UNUSED(next)
    return false;
}

QString QWebFrame::evaluateJavaScript(const QString& scriptSource)
{
    KJSProxy *proxy = d->frame->scriptProxy();
    QString rc;
    if (proxy) {
        KJS::JSValue *v = proxy->evaluate(String(), 0, scriptSource, d->frame->document());
        if (v) {
            rc = String(v->toString(proxy->interpreter()->globalExec()));
        }
    }
    return rc;
}
