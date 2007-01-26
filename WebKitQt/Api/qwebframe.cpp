/*
    Copyright (C) 2007 Trolltech ASA

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

#include <qdebug.h>

#include "FrameLoaderClientQt.h"
#include "FrameQtClient.h"
#include "FrameQt.h"
#include "FrameView.h"
#include "ResourceRequest.h"

#include "markup.h"
#include "RenderTreeAsText.h"
#include "Element.h"
#include "Document.h"
#include "RenderObject.h"

#include "bindings/runtime.h"
#include "bindings/runtime_root.h"
#include "ExecState.h"
#include "object.h"

#include "wtf/HashMap.h"

using namespace WebCore;

QWebFrame::QWebFrame(QWebPage *parent, QWebFrameData *frameData)
    : QScrollArea(parent)
    , d(new QWebFramePrivate)
{
    d->page = parent;

    d->frameLoaderClient = new FrameLoaderClientQt();
    d->frame = new FrameQt(parent->d->page, frameData->ownerElement, new FrameQtClient(), d->frameLoaderClient);
    d->frameLoaderClient->setFrame(this, d->frame.get());

    d->frameView = new FrameView(d->frame.get());
    d->frameView->deref();
    d->frameView->setScrollArea(this);
    d->frame->setView(d->frameView.get());
    if (!frameData->url.isEmpty()) {
        ResourceRequest request(frameData->url, frameData->referrer);
        d->frame->loader()->load(request, frameData->name);
    }
}


QWebFrame::QWebFrame(QWebFrame *parent, QWebFrameData *frameData)
    : QScrollArea(parent->widget())
    , d(new QWebFramePrivate)
{
    d->page = parent->d->page;

    d->frameLoaderClient = new FrameLoaderClientQt();
    d->frame = new FrameQt(parent->d->page->d->page, frameData->ownerElement, new FrameQtClient(), d->frameLoaderClient);
    d->frameLoaderClient->setFrame(this, d->frame.get());

    d->frameView = new FrameView(d->frame.get());
    d->frameView->deref();
    d->frameView->setScrollArea(this);
    d->frame->setView(d->frameView.get());
}

QWebFrame::~QWebFrame()
{
    if (d->frameView)
        d->frameView->setScrollArea(0);
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


QWebPage * QWebFrame::page() const
{
    return d->page;
}

void QWebFrame::resizeEvent(QResizeEvent *e)
{
    QScrollArea::resizeEvent(e);
    RenderObject *renderer = d->frame->renderer();
    if (renderer)
        renderer->setNeedsLayout(true);
    d->frameView->scheduleRelayout();
}

#include "qwebframe.moc"
