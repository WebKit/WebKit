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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/
#ifndef QWEBVIEW_H
#define QWEBVIEW_H

#include "qwebkitglobal.h"
#include "qwebpage.h"
#include <QtGui/qwidget.h>

class QWebPage;
class QWebViewPrivate;

class QWEBKIT_EXPORT QWebView : public QWidget
{
    Q_OBJECT
public:
    explicit QWebView(QWidget *parent = 0);
    virtual ~QWebView();

    QWebPage *page() const;
    void setPage(QWebPage *page);

    // ### also offer in QWebFrame
    void load(const QUrl &url);
    void load(const QWebNetworkRequest &request);
    void setHtml(const QString &html, const QUrl &baseUrl = QUrl());
    void setHtml(const QByteArray &html, const QUrl &baseUrl = QUrl());

    QWebPageHistory *history() const;
    QWebSettings *settings() const;

    QString title() const;
    QUrl url() const;
    QPixmap icon() const;

    QString selectedText() const;

    QAction *action(QWebPage::WebAction action) const;
    void triggerAction(QWebPage::WebAction action, bool checked = false);

    bool isModified();

    Qt::TextInteractionFlags textInteractionFlags() const;
    void setTextInteractionFlags(Qt::TextInteractionFlags flags);

    /* #### QTextBrowser compatibility?
    bool openLinks() const;
    void setOpenLinks(bool open);

    bool openExternalLinks() const;
    void setOpenExternalLinks(bool open);
    */

    QSize sizeHint() const;
public Q_SLOTS:
    void stop();
    void backward();
    void forward();
    void reload();

Q_SIGNALS:
    void loadStarted();
    void loadProgressChanged(int progress);
    void loadFinished();
    void titleChanged(const QString& title);
    void statusBarTextChanged(const QString& text);
    void linkClicked(const QUrl &url);
    void selectionChanged();
    void iconLoaded();

protected:
    virtual void resizeEvent(QResizeEvent *e);

private:
    QWebViewPrivate *d;
};

#endif // QWEBVIEW_H
