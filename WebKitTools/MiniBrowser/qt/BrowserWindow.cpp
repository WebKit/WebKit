/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2010 University of Szeged
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "BrowserWindow.h"
#include <WebKit2/WKPageNamespace.h>
#include <WebKit2/qwkpage.h>

static QWKPage* createNewPage(QWKPage* page)
{
    return page;
}

BrowserView::BrowserView(QWidget* parent)
    : QGraphicsView(parent)
    , m_item(0)
{
    m_context.adopt(WKContextGetSharedProcessContext());

    WKRetainPtr<WKPageNamespaceRef> pageNamespace(AdoptWK, WKPageNamespaceCreate(m_context.get()));

    m_item = new QGraphicsWKView(pageNamespace.get(), QGraphicsWKView::Simple, 0);
    setScene(new QGraphicsScene(this));
    scene()->addItem(m_item);

    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    connect(m_item, SIGNAL(titleChanged(QString)), this, SLOT(setWindowTitle(QString)));
    m_item->page()->setCreateNewPageFunction(createNewPage);
}

void BrowserView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    QRectF rect(QPoint(0, 0), event->size());
    m_item->setGeometry(rect);
    scene()->setSceneRect(rect);
}

void BrowserView::load(const QUrl& url)
{
    m_item->load(url);
}

QGraphicsWKView* BrowserView::view() const
{
    return m_item;
}

BrowserWindow::BrowserWindow()
{
    m_menu = new QMenuBar();
    m_browser = new BrowserView();
    m_addressBar = new QLineEdit();

    m_menu->addAction("Quit", this, SLOT(close()));

    m_browser->setFocus(Qt::OtherFocusReason);

    connect(m_addressBar, SIGNAL(returnPressed()), SLOT(changeLocation()));

    QToolBar* bar = addToolBar("Navigation");
    bar->addAction(m_browser->view()->page()->action(QWKPage::Back));
    bar->addAction(m_browser->view()->page()->action(QWKPage::Forward));
    bar->addAction(m_browser->view()->page()->action(QWKPage::Reload));
    bar->addAction(m_browser->view()->page()->action(QWKPage::Stop));
    bar->addWidget(m_addressBar);

    this->setMenuBar(m_menu);
    this->setCentralWidget(m_browser);

    m_browser->setFocus(Qt::OtherFocusReason);
}

void BrowserWindow::load(const QString& url)
{
    m_addressBar->setText(url);
    m_browser->load(QUrl(url));
}

void BrowserWindow::changeLocation()
{
    QString string = m_addressBar->text();
    m_browser->load(string);
}

BrowserWindow::~BrowserWindow()
{
    delete m_addressBar;
    delete m_browser;
    delete m_menu;
}
