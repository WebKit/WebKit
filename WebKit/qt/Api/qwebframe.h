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

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#ifndef QWEBFRAME_H
#define QWEBFRAME_H

#include <QtCore/qobject.h>
#include <QtCore/qurl.h>
#if QT_VERSION >= 0x040400
#include <QtNetwork/qnetworkaccessmanager.h>
#endif
#include "qwebkitglobal.h"

class QRect;
class QPoint;
class QPainter;
class QPixmap;
class QMouseEvent;
class QWheelEvent;
class QWebNetworkRequest;
class QNetworkRequest;

class QWebFramePrivate;
class QWebPage;
class QRegion;

namespace WebCore {
    class WidgetPrivate;
    class FrameLoaderClientQt;
    class ChromeClientQt;
}
class QWebFrameData;

class QWEBKIT_EXPORT QWebFrame : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Qt::ScrollBarPolicy verticalScrollBarPolicy READ verticalScrollBarPolicy WRITE setVerticalScrollBarPolicy)
    Q_PROPERTY(Qt::ScrollBarPolicy horizontalScrollBarPolicy READ horizontalScrollBarPolicy WRITE setHorizontalScrollBarPolicy)
private:
    QWebFrame(QWebPage *parent, QWebFrameData *frameData);
    QWebFrame(QWebFrame *parent, QWebFrameData *frameData);
    ~QWebFrame();

public:
    QWebPage *page() const;

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

    void addToJSWindowObject(const QString &name, QObject *object);
    QString markup() const;
    QString innerText() const;
    QString renderTreeDump() const;

    QString title() const;
    QUrl url() const;
    QPixmap icon() const;
    
    QString name() const;

    QWebFrame *parentFrame() const;
    QList<QWebFrame*> childFrames() const;

    Qt::ScrollBarPolicy verticalScrollBarPolicy() const;
    void setVerticalScrollBarPolicy(Qt::ScrollBarPolicy);
    Qt::ScrollBarPolicy horizontalScrollBarPolicy() const;
    void setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy);

    void render(QPainter *painter, const QRegion &clip);
    void layout();

    QPoint pos() const;
    QRect geometry() const;

public Q_SLOTS:
    QString evaluateJavaScript(const QString& scriptSource);

Q_SIGNALS:
    void cleared();
    void loadDone(bool ok);
    void provisionalLoad();
    void titleChanged(const QString &title);
    void hoveringOverLink(const QString &link, const QString &title, const QString &textContent);
    void urlChanged(const QUrl &url);

    void loadStarted();
    void loadFinished();

    /**
      * Signal is emitted when the mainframe()'s initial layout is completed.
     */
    void initialLayoutComplete();
    
    /**
     * Signal is emitted when an icon ("favicon") is loaded from the site.
     */
    void iconLoaded();

private:
    friend class QWebPage;
    friend class QWebPagePrivate;
    friend class QWebFramePrivate;
    friend class WebCore::WidgetPrivate;
    friend class WebCore::FrameLoaderClientQt;
    friend class WebCore::ChromeClientQt;
    QWebFramePrivate *d;
};

#endif
