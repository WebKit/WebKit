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
#include <QtGui/qicon.h>
#include <QtCore/qurl.h>
#if QT_VERSION >= 0x040400
#include <QtNetwork/qnetworkaccessmanager.h>
#endif

QT_BEGIN_NAMESPACE
class QNetworkRequest;
QT_END_NAMESPACE

class QWebPage;
class QWebViewPrivate;
class QWebNetworkRequest;

class QWEBKIT_EXPORT QWebView : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title)
    Q_PROPERTY(QUrl url READ url WRITE load)
    Q_PROPERTY(QIcon icon READ icon)
    Q_PROPERTY(QString selectedText READ selectedText)
    Q_PROPERTY(bool modified READ isModified)
    //Q_PROPERTY(Qt::TextInteractionFlags textInteractionFlags READ textInteractionFlags WRITE setTextInteractionFlags)
    Q_PROPERTY(qreal textSizeMultiplier READ textSizeMultiplier WRITE setTextSizeMultiplier)
public:
    explicit QWebView(QWidget *parent = 0);
    virtual ~QWebView();

    QWebPage *page() const;
    void setPage(QWebPage *page);

    void load(const QUrl &url);
#if QT_VERSION < 0x040400
    void load(const QWebNetworkRequest &request);
#else
    void load(const QNetworkRequest &request,
              QNetworkAccessManager::Operation operation = QNetworkAccessManager::GetOperation,
              const QByteArray &body = QByteArray());
#endif
    void setHtml(const QString &html, const QUrl &baseUrl = QUrl());
    void setHtml(const QByteArray &html, const QUrl &baseUrl = QUrl());
    void setContent(const QByteArray &data, const QString &mimeType = QString(), const QUrl &baseUrl = QUrl());

    QWebHistory *history() const;
    QWebSettings *settings() const;

    QString title() const;
    QUrl url() const;
    QIcon icon() const;

    QString selectedText() const;

    QAction *pageAction(QWebPage::WebAction action) const;
    void triggerPageAction(QWebPage::WebAction action, bool checked = false);

    bool isModified() const;

    /*
    Qt::TextInteractionFlags textInteractionFlags() const;
    void setTextInteractionFlags(Qt::TextInteractionFlags flags);
    void setTextInteractionFlag(Qt::TextInteractionFlag flag);
    */

    QVariant inputMethodQuery(Qt::InputMethodQuery property) const;

    QSize sizeHint() const;

    void setTextSizeMultiplier(qreal factor);
    qreal textSizeMultiplier() const;

    bool findText(const QString &subString, QWebPage::FindFlags options = 0);

public Q_SLOTS:
    void stop();
    void back();
    void forward();
    void reload();

Q_SIGNALS:
    void loadStarted();
    void loadProgress(int progress);
    void loadFinished();
    void titleChanged(const QString& title);
    void statusBarMessage(const QString& text);
    void linkClicked(const QUrl &url);
    void selectionChanged();
    void iconChanged();
    void urlChanged(const QUrl &url);

protected:
    void resizeEvent(QResizeEvent *e);
    void paintEvent(QPaintEvent *ev);

    virtual QWebView *createWindow(QWebPage::WebWindowType type);

    virtual void mouseMoveEvent(QMouseEvent*);
    virtual void mousePressEvent(QMouseEvent*);
    virtual void mouseDoubleClickEvent(QMouseEvent*);
    virtual void mouseReleaseEvent(QMouseEvent*);
    virtual void contextMenuEvent(QContextMenuEvent*);
    virtual void wheelEvent(QWheelEvent*);
    virtual void keyPressEvent(QKeyEvent*);
    virtual void keyReleaseEvent(QKeyEvent*);
    virtual void dragEnterEvent(QDragEnterEvent *);
    virtual void dragLeaveEvent(QDragLeaveEvent *);
    virtual void dragMoveEvent(QDragMoveEvent *);
    virtual void dropEvent(QDropEvent *);
    virtual void focusInEvent(QFocusEvent*);
    virtual void focusOutEvent(QFocusEvent*);
    virtual void inputMethodEvent(QInputMethodEvent*);

    virtual bool focusNextPrevChild(bool next);

private:
    friend class QWebPage;
    QWebViewPrivate *d;
};

#endif // QWEBVIEW_H
