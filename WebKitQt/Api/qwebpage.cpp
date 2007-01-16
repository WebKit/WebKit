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

#include "qwebpage.h"
#include "qwebframe.h"
#include "qwebpage_p.h"
#include "qwebframe_p.h"

#include <qurl.h>

#include "FrameQt.h"
#include "ChromeClientQt.h"
#include "ContextMenuClientQt.h"
#include "EditorClientQt.h"
#include "Page.h"
#include "FrameLoader.h"
#include "KURL.h"
#include "qboxlayout.h"

using namespace WebCore;

QWebPagePrivate::QWebPagePrivate(QWebPage *qq)
    : q(qq)
{
    chromeClient = new ChromeClientQt(q);
    contextMenuClient = new ContextMenuClientQt();
    editorClient = new EditorClientQt();
    page = new Page(chromeClient, contextMenuClient, editorClient);

    mainFrame = 0;
}

QWebPagePrivate::~QWebPagePrivate()
{
    delete page;
}

void QWebPagePrivate::createMainFrame()
{
    if (!mainFrame) {
        mainFrame = q->createFrame(0);
        layout->addWidget(mainFrame);
    }
}


QWebPage::QWebPage(QWidget *parent)
    : QWidget(parent)
    , d(new QWebPagePrivate(this))
{
    d->layout = new QVBoxLayout(this);
    d->layout->setMargin(0);
    d->layout->setSpacing(0);
}

QWebPage::~QWebPage()
{
    delete d;
}

QWebFrame *QWebPage::createFrame(QWebFrame *parentFrame)
{
    if (parentFrame)
        return new QWebFrame(parentFrame);
    return new QWebFrame(this);
}

void QWebPage::open(const QUrl &url)
{
    d->createMainFrame();

    d->mainFrame->d->frame->loader()->load(KURL(url.toString()));
}

QWebFrame *QWebPage::mainFrame() const
{
    d->createMainFrame();
    return d->mainFrame;
}


QSize QWebPage::sizeHint() const
{
    return QSize(800, 600);
}

#include "qwebpage.moc"

