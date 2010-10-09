/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 University of Szeged. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "PlatformWebView.h"
#include "qgraphicswkview.h"
#include <QtGui>

namespace WTR {

class WebView : public QGraphicsView {
public:
    WebView(WKPageNamespaceRef);

    QGraphicsWKView* wkView() const { return m_item; }

    virtual ~WebView() { delete m_item; }

private:
    QGraphicsWKView* m_item;
};

WebView::WebView(WKPageNamespaceRef namespaceRef)
    : QGraphicsView()
    , m_item(new QGraphicsWKView(namespaceRef))
{
    setScene(new QGraphicsScene(this));
    scene()->addItem(m_item);
}

PlatformWebView::PlatformWebView(WKPageNamespaceRef namespaceRef)
    : m_view(new WebView(namespaceRef))
    , m_window(new QMainWindow())
{
    m_view->setParent(m_window);
    m_window->setCentralWidget(m_view);
    m_window->setGeometry(0, 0, 800, 600);
}

PlatformWebView::~PlatformWebView()
{
    delete m_window;
}

void PlatformWebView::resizeTo(unsigned width, unsigned height)
{
    m_window->resize(width, height);
}

WKPageRef PlatformWebView::page()
{
    return m_view->wkView()->page()->pageRef();
}

void PlatformWebView::focus()
{
    m_view->setFocus(Qt::OtherFocusReason);
}

WKRect PlatformWebView::windowFrame()
{
    // Implement.

    WKRect wkFrame;
    wkFrame.origin.x = 0;
    wkFrame.origin.y = 0;
    wkFrame.size.width = 0;
    wkFrame.size.height = 0;
    return wkFrame;
}

void PlatformWebView::setWindowFrame(WKRect)
{
    // Implement.
}

} // namespace WTR
