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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "qwebview.h"
#include "qwebframe.h"
#include "qevent.h"

class QWebViewPrivate
{
public:
    QWebPage *page;
};

QWebView::QWebView(QWidget *parent)
    : QWidget(parent)
{
    d = new QWebViewPrivate;
    d->page = 0;
}

QWebView::~QWebView()
{
    if (d->page && d->page->parent() == this)
        delete d->page;
    delete d;
}

QWebPage *QWebView::page() const
{
    if (!d->page) {
        QWebView *that = const_cast<QWebView *>(this);
        that->setPage(new QWebPage(that));
    }
    return d->page;
}

void QWebView::setPage(QWebPage *page)
{
    if (d->page == page)
        return;
    if (d->page) {
        if (d->page->parent() == this) {
            delete d->page;
        } else {
            d->page->disconnect(this);
        }
    }
    d->page = page;
    if (d->page) {
        // #### connect signals
        QWebFrame *mainFrame = d->page->mainFrame();
        connect(mainFrame, SIGNAL(loadStarted()),
                this, SIGNAL(loadStarted()));
        connect(mainFrame, SIGNAL(loadFinished()),
                this, SIGNAL(loadFinished()));
        connect(mainFrame, SIGNAL(titleChanged(const QString&)),
                this, SIGNAL(titleChanged(const QString&)));
        connect(mainFrame, SIGNAL(iconLoaded()),
                this, SIGNAL(iconLoaded()));

        connect(d->page, SIGNAL(loadProgressChanged(int)),
                this, SIGNAL(loadProgressChanged(int)));
        connect(d->page, SIGNAL(statusBarTextChanged(const QString &)),
                this, SIGNAL(statusBarTextChanged(const QString &)));
    }
    update();
}

void QWebView::load(const QUrl &url)
{
    page()->mainFrame()->load(url);
}

void QWebView::load(const QWebNetworkRequest &request)
{
    page()->mainFrame()->load(request);
}

void QWebView::setHtml(const QString &html, const QUrl &baseUrl)
{
    page()->mainFrame()->setHtml(html, baseUrl);
}

void QWebView::setHtml(const QByteArray &html, const QUrl &baseUrl)
{
    page()->mainFrame()->setHtml(html, baseUrl);
}

void QWebView::setContent(const QByteArray &data, const QString &mimeType, const QUrl &baseUrl)
{
    page()->mainFrame()->setContent(data, mimeType, baseUrl);
}

QWebPageHistory *QWebView::history() const
{
    return page()->history();
}

QWebSettings *QWebView::settings() const
{
    return page()->settings();
}

QString QWebView::title() const
{
    if (d->page)
        return d->page->title();
    return QString();
}

QUrl QWebView::url() const
{
    if (d->page)
        return d->page->url();
    return QUrl();
}

QPixmap QWebView::icon() const
{
    if (d->page)
        return d->page->icon();
    return QPixmap();
}

QString QWebView::selectedText() const
{
    if (d->page)
        return d->page->selectedText();
    return QString();
}

QAction *QWebView::action(QWebPage::WebAction action) const
{
    return page()->action(action);
}

void QWebView::triggerAction(QWebPage::WebAction action, bool checked)
{
    page()->triggerAction(action, checked);
}

bool QWebView::isModified()
{
    if (d->page)
        return d->page->isModified();
    return false;
}

Qt::TextInteractionFlags QWebView::textInteractionFlags() const
{
    // ### FIXME (add to page)
    return Qt::TextInteractionFlags();
}

void QWebView::setTextInteractionFlags(Qt::TextInteractionFlags flags)
{
    Q_UNUSED(flags)
    // ### FIXME (add to page)
}

QSize QWebView::sizeHint() const
{
    return QSize(800, 600); // ####...
}

void QWebView::stop()
{
    if (d->page)
        d->page->triggerAction(QWebPage::Stop);
}

void QWebView::backward()
{
    if (d->page)
        d->page->triggerAction(QWebPage::GoBack);
}

void QWebView::forward()
{
    if (d->page)
        d->page->triggerAction(QWebPage::GoForward);
}

void QWebView::reload()
{
    if (d->page)
        d->page->triggerAction(QWebPage::Reload);
}

void QWebView::resizeEvent(QResizeEvent *e)
{
    if (d->page)
        d->page->resize(e->size());
}

