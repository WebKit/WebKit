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

#include "markup.h"
#include "RenderTreeAsText.h"
#include "Element.h"
#include "Document.h"

#include "bindings/runtime.h"
#include "bindings/runtime_root.h"
#include "ExecState.h"
#include "object.h"


using namespace WebCore;

QWebFrame::QWebFrame(QWebPage *parent)
    : QScrollArea(parent)
    , d(new QWebFramePrivate)
{
    d->frameLoaderClient = new FrameLoaderClientQt();
    d->frame = new FrameQt(parent->d->page, 0, new FrameQtClient(), d->frameLoaderClient);
    d->frameLoaderClient->setFrame(this, d->frame);

    d->frameView = new FrameView(d->frame);
    d->frameView->setScrollArea(this);
    d->frame->setView(d->frameView);
}


QWebFrame::QWebFrame(QWebFrame *parent)
    : QScrollArea(parent)
    , d(new QWebFramePrivate)
{
//     d->frameLoaderClient = new FrameLoaderClientQt();
//     d->frame = new FrameQt(page, 0, new FrameQtClient(), frameLoaderClient);
//     d->frameLoaderClient->setFrame(d->frame);

//     d->frameView = new FrameView(d->frame);
//     d->frameView->setScrollArea(this);
//     d->frame->setView(d->frameView);
}

QWebFrame::~QWebFrame()
{
    delete d->frame;
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
    Element *documentElement = d->frame->document()->documentElement();
    return documentElement->innerText();
}

QString QWebFrame::renderTreeDump() const
{
    return externalRepresentation(d->frame->renderer());
}


#include "qwebframe.moc"
