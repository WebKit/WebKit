/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Holger Hans Peter Freyther
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorClientQt.h"

#include "qwebpage.h"
#include "qwebpage_p.h"
#include "qwebview.h"

#include <QtCore/QCoreApplication>

#include "InspectorController.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformString.h"

namespace WebCore {

class InspectorClientWebPage : public QWebPage
{
    Q_OBJECT
    friend class InspectorClientQt;
public:
    QWebPage* createWindow(QWebPage::WebWindowType)
    {
        QWidget *w = new QWebView(0);
        QWebPage *page = new QWebPage(w);
        page->setView(w);
        connect(page, SIGNAL(destroyed()), w, SLOT(deleteLater()));
        return page;
    }

Q_SIGNALS:
    void attachRequested();
    void detachRequested();
};


class InspectorClientView : public QWebView {
public:
    InspectorClientView(InspectorController* controller)
        : QWebView(0)
        , m_controller(controller)
    {
        setPage(new InspectorClientWebPage);
        connect(page(), SIGNAL(destroyed()), SLOT(deleteLater()));
    }

protected:

    void closeEvent(QCloseEvent* ev)
    {
        QWidget::closeEvent(ev);
        m_controller->setWindowVisible(false);
    }

private:
    InspectorController* m_controller;
};


InspectorClientQt::InspectorClientQt(QWebPage* page)
    : m_inspectedWebPage(page)
{}

void InspectorClientQt::inspectorDestroyed()
{
    delete this;
}

Page* InspectorClientQt::createPage()
{
    if (m_webPage)
        return m_webPage->d->page;

    InspectorClientView* view = new InspectorClientView(m_inspectedWebPage->d->page->inspectorController());
    m_webPage.set(qobject_cast<InspectorClientWebPage*>(view->page()));
    m_webPage->mainFrame()->load(QString::fromLatin1("qrc:/webkit/inspector/inspector.html"));
    m_webPage->view()->setMinimumSize(400,300);
    return m_webPage->d->page;
}

String InspectorClientQt::localizedStringsURL()
{
    notImplemented();
    return String();
}

String InspectorClientQt::hiddenPanels()
{
    notImplemented();
    return String();
}

void InspectorClientQt::showWindow()
{
    if (!m_webPage)
        return;

    updateWindowTitle();
    m_webPage->view()->show();
    m_inspectedWebPage->d->page->inspectorController()->setWindowVisible(true);
}

void InspectorClientQt::closeWindow()
{
    if (!m_webPage)
        return;

    m_webPage->view()->hide();
    m_inspectedWebPage->d->page->inspectorController()->setWindowVisible(false);
}

bool InspectorClientQt::windowVisible()
{
    if (!m_webPage)
        return false;
    return m_webPage->view()->isVisible();
}

void InspectorClientQt::attachWindow()
{
    if (!m_webPage)
        return;

    emit m_webPage->attachRequested();
}

void InspectorClientQt::detachWindow()
{
    if (!m_webPage)
        return;

    emit m_webPage->detachRequested();
}

void InspectorClientQt::setAttachedWindowHeight(unsigned height)
{
    notImplemented();
}

void InspectorClientQt::highlight(Node* node)
{
    notImplemented();
}

void InspectorClientQt::hideHighlight()
{
    notImplemented();
}

void InspectorClientQt::inspectedURLChanged(const String& newURL)
{
    m_inspectedURL = newURL;
    updateWindowTitle();
}

void InspectorClientQt::updateWindowTitle()
{
    if (!m_webPage)
        return;

    QString caption = QCoreApplication::translate("QWebPage", "Web Inspector - %2");
    m_webPage->view()->setWindowTitle(caption.arg(m_inspectedURL));
}

void InspectorClientQt::populateSetting(const String& key, InspectorController::Setting& setting)
{
    notImplemented();
}

void InspectorClientQt::storeSetting(const String& key, const InspectorController::Setting& setting)
{
    notImplemented();
}

void InspectorClientQt::removeSetting(const String& key)
{
    notImplemented();
}

}

#include "InspectorClientQt.moc"
